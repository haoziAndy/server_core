#include "stdafx.h"
#include "websocket_server.h"
#include "websocket_connection.h"
#include "msg_header.h"

namespace z {
namespace net {

WebsocketConnection::WebsocketConnection(WebsocketServer* server, int conn_index, boost::asio::ip::tcp::socket&& sock)
    : IConnection(server, boost::asio::ip::tcp::socket(server->io_service()), conn_index)
#ifdef ENABLE_WEBSOCKET_SSL
    ,ws_(std::move(sock), server->ssl_ctx())
#else
    , ws_(std::move(sock))
#endif
    , idle_count_(0)
    , msg_count_(0)
{

}

WebsocketConnection::~WebsocketConnection(){
}

void WebsocketConnection::Start(){
    deadline_timer_.expires_from_now(boost::posix_time::seconds(WEBSOCKET_SERVER.login_time_out_sec()));
    deadline_timer_.async_wait([this, self=shared_from_this()](const boost::system::error_code& ec){
        if (!ec){
            // timeout 还处于账户登录, 断开
            if (status_ <= LoginStatus_ACCOUNT_LOGIN){
                LOG_DEBUG("session[%d] OnLoginTimeOut. close.", session_id());
                AsyncClose();
                return;
            }else{
                idle_count_ = 0;
                msg_count_ = 0;
                StartKeepAliveTimer();
            }
        }
    });

    boost::asio::dispatch(ws_.get_executor(), [this, self = shared_from_this()](){
        auto on_accept = [this, self](boost::beast::error_code ec) {
            if (ec) {
                if (ec != boost::beast::websocket::error::closed) {
                    LOG_ERR("WebsocketConnection::on_accept error, errorcode[%d]:%s", ec.value(), ec.message().c_str());
                }

                AsyncClose();
                return;
            }

            // Read a message
            do_read();
            };

#ifdef ENABLE_WEBSOCKET_SSL
        // Set the timeout.
        boost::beast::get_lowest_layer(ws_).expires_after(std::chrono::seconds(30));
        // Perform the SSL handshake
        ws_.next_layer().async_handshake(boost::asio::ssl::stream_base::server, [this, self, on_accept](boost::beast::error_code ec) {
            if (ec) {
                LOG_ERR("WebsocketConnection::on_handshake error, errorcode[%d]:%s", ec.value(), ec.message().c_str());
                AsyncClose();
                return;
            }

            // Turn off the timeout on the tcp_stream, because
            // the websocket stream has its own timeout system.
            boost::beast::get_lowest_layer(ws_).expires_never();

            // Set suggested timeout settings for the websocket
            ws_.set_option(
                boost::beast::websocket::stream_base::timeout::suggested(
                    boost::beast::role_type::server));

            // Set a decorator to change the Server of the handshake
            ws_.set_option(boost::beast::websocket::stream_base::decorator(
                [](boost::beast::websocket::response_type& res)
                {
                    res.set(boost::beast::http::field::server,
                        std::string(BOOST_BEAST_VERSION_STRING) +
                        " websocket-server-async-ssl");
                }));

            // Accept the websocket handshake
            ws_.async_accept(on_accept);
            });
#else
        // Turn off the timeout on the tcp_stream, because
        // the websocket stream has its own timeout system.
        boost::beast::get_lowest_layer(ws_).expires_never();

        ws_.set_option(
            boost::beast::websocket::stream_base::timeout::suggested(boost::beast::role_type::server));

        // Set a decorator to change the Server of the handshake
        ws_.set_option(boost::beast::websocket::stream_base::decorator(
            [](boost::beast::websocket::response_type& res)
            {
                res.set(boost::beast::http::field::server,
                    std::string(BOOST_BEAST_VERSION_STRING) +
                    " websocket-server-async");
            }));
        // Accept the websocket handshake
        ws_.async_accept(on_accept);
#endif
    });
}

void WebsocketConnection::StartKeepAliveTimer(){
    deadline_timer_.expires_from_now(boost::posix_time::seconds(1));
    deadline_timer_.async_wait([this, self=shared_from_this()](const boost::system::error_code& ec){
        if (!ec){
            // 判断连接5分钟没有消息就断开
            auto timeout_sec = WEBSOCKET_SERVER.keepalive_time_out_sec();
            if (idle_count_ >= timeout_sec){
                LOG_DEBUG("session[%d] OnKeepAliveTimeOut. idle >= %d. close.", session_id(), timeout_sec);
                AsyncClose();
                return;
            }else{
                // 判断连接5分钟没有消息就断开
                enum {idle_max_count = 300};
                if (idle_count_ >= idle_max_count){
                    LOG_DEBUG("session[%d] OnKeepAliveTimeOut. idle >= %d. close.", session_id(), idle_max_count);
                    AsyncClose();
                    return;
                }
                // 消息过多, 断开 TODO(zen): 设置 数目
                enum { max_msg_flood = 1024 };
                if (msg_count_ >= max_msg_flood){
                    LOG_DEBUG("session[%d] recv too many msgs. msg > %d per sec. close.", session_id(), max_msg_flood);
                    AsyncClose();
                    return;
                }
            }
            
            ++idle_count_;
            msg_count_ = 0;
            StartKeepAliveTimer();
        }
    });
}

void WebsocketConnection::do_read(){
    // Read a message into our buffer
    ws_.async_read(buffer_, [this, self=shared_from_this()](boost::beast::error_code ec, std::size_t bytes_transferred){
        if(ec){
            AsyncClose();
            if(ec != boost::beast::websocket::error::closed && ec != boost::asio::error::eof){
                LOG_ERR("WebsocketConnection::on_read error, errorcode[%d]:%s", ec.value(), ec.message().c_str());
            }
            return;
        }
        boost::ignore_unused(bytes_transferred);

        auto buffer_data = buffer_.data();
        auto buff_size = boost::asio::buffer_size(buffer_data);
        if (buff_size < sizeof(CMsgHeader)){
            AsyncClose();
            LOG_ERR("WebsocketConnection::on_read received incomplete client package, buff_size: %d < msg_head_size: %d", buff_size, sizeof(CMsgHeader));
            return;
        }

        std::vector<unsigned char> byte_array(buff_size);
        boost::asio::buffer_copy(boost::asio::buffer(byte_array), buffer_data);
        buffer_.clear();
        CMsgHeader* msg = reinterpret_cast<CMsgHeader*>(byte_array.data());
        if(byte_array.size() != buff_size){
            AsyncClose();
            LOG_ERR("WebsocketConnection::on_read copy data error, buff_size: %d != byte_array_size: %d", buff_size, byte_array.size());
            return;
        }
        CMsgHeaderNtoh(msg);

        if (msg->length > recv_client_message_max_length){
            LOG_ERR("WebsocketConnection::on_read Client msg length %d > %d", msg->length, recv_client_message_max_length);
            AsyncClose();
            return;
        }

        int msg_length = sizeof(*msg) + msg->length;
        if (buff_size != msg_length){ // 不够一个完整的包
            LOG_ERR("WebsocketConnection::on_read received incomplete client package, msg_length: %d != buff_size: %d", msg_length, buff_size);
            AsyncClose();
            return;
        }

        int h_ret = WEBSOCKET_SERVER.request_handler()->OnMessage(this, msg);
        // 消息处理 返回-1, 表明应该断开连接
        if (h_ret < 0)
        {
            AsyncClose();
            return;
        }

        if (status_ == LoginStatus_DEFAULT){
            status_ = LoginStatus_ACCOUNT_LOGIN;
        }

        // 设置idle_count
        idle_count_ = 0;
        ++ msg_count_;

        // Do another read
        do_read();
    });
}

void WebsocketConnection::StartWrite(){
    if (is_closing_)
        return;

    if (is_writing_)
        return;

    if (send_queue_.empty())
        return;

    is_writing_ = true;

    ws_.text(false);
    ws_.async_write(send_queue_, [this, self=shared_from_this()](const boost::system::error_code& ec, size_t bytes_transferred){
        is_writing_ = false;
        if(ec){
            AsyncClose();
            if(ec != boost::beast::websocket::error::closed && ec != boost::asio::error::eof){
                LOG_ERR("WebsocketConnection::on_read error, errorcode[%d]:%s", ec.value(), ec.message().c_str());
            }
            return;
        }

        OnWrite(bytes_transferred);
        send_queue_.clear();
        if (!pending_send_queue_.empty()){
            send_queue_.swap(pending_send_queue_);
            // continue write
            StartWrite();
        }
    });
}

void WebsocketConnection::Close(){
    is_closing_ = true;
    deadline_timer_.cancel();
    if (ws_.is_open()) {
        //LOG_INFO("WebsocketConnection::Close async_close");
        ws_.async_close(boost::beast::websocket::close_code::normal, [this, self=shared_from_this()](boost::beast::error_code const &ec){
            if (ec) {
                LOG_ERR("WebsocketConnection::Close error, errorcode[%d]:%s", ec.value(), ec.message().c_str());
            }
            OnClosed();
        });
    }

    WEBSOCKET_SERVER.request_handler()->OnClientDisconnect(this);
}

} // namespace net
} // namespace z