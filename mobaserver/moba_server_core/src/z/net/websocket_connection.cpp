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
        auto do_http_read = [this, self]() {
            // 设置读取 HTTP Upgrade 请求的临时超时（防止恶意死连接占用）
            boost::beast::get_lowest_layer(ws_).expires_after(std::chrono::seconds(15));

            // 在堆上动态分配 http_req，利用智能指针捕获，握手完自动销毁
            auto http_req = std::make_shared<boost::beast::http::request<boost::beast::http::string_body>>();

            // 【Boost 1.73 核心适配点】：WSS 和 WS 下统一安全地传入各自对应的紧邻下一层（Next Layer）
            auto& next_io_layer = ws_.next_layer(); 

            // 异步读取 HTTP 请求
            boost::beast::http::async_read(
                next_io_layer, 
                buffer_, 
                *http_req, 
                [this, self, http_req](boost::beast::error_code ec, std::size_t) {
                    if (ec) {
                        LOG_ERR("session[%d] HTTP read error: %s", session_id(), ec.message().c_str());
                        AsyncClose();
                        return;
                    }

                    // 强校验：如果不是标准的 WebSocket 升级请求，直接拒绝
                    if (!boost::beast::websocket::is_upgrade(*http_req)) {
                        LOG_ERR("session[%d] Not a valid websocket upgrade request.", session_id());
                        AsyncClose();
                        return;
                    }

                    // 数据读取无误，进入 WebSocket 协议升级
                    // Turn off the timeout on the tcp_stream, because
                    // the websocket stream has its own timeout system.
                    boost::beast::get_lowest_layer(ws_).expires_never();

                    // Set suggested timeout settings for the websocket
                    ws_.set_option(
                        boost::beast::websocket::stream_base::timeout::suggested(
                            boost::beast::role_type::server));

                    // Set a decorator to change the Server of the handshake
                    ws_.set_option(boost::beast::websocket::stream_base::decorator(
                        [](boost::beast::websocket::response_type& res){
#ifdef ENABLE_WEBSOCKET_SSL
                            res.set(boost::beast::http::field::server,
                                std::string(BOOST_BEAST_VERSION_STRING) + " websocket-server-async-ssl");
#else
                            res.set(boost::beast::http::field::server,
                                std::string(BOOST_BEAST_VERSION_STRING) + " websocket-server-async");
#endif
                        }));

                    // Accept the websocket handshake
                    ws_.async_accept(*http_req, [this, self, http_req](boost::beast::error_code ec) {
                        if (ec) {
                            if (ec != boost::beast::websocket::error::closed) {
                                LOG_ERR("WebsocketConnection::on_accept error, errorcode[%d]:%s", ec.value(), ec.message().c_str());
                            }

                            AsyncClose();
                            return;
                        }

                        {
                            std::string real_client_ip;

                            // 【第一级：X-Real-IP】
                            // 很多云网关或 Nginx 会被配置为直接将客户端真实 IP 写入该字段（全小写查找）
                            auto it_real = http_req->find("x-real-ip");
                            if (it_real != http_req->end() && !it_real->value().empty()) {
                                real_client_ip = std::string(it_real->value());
                            }

                            // 【第二级：X-Forwarded-For】
                            // 标准的反向代理链条头部。如果有多次代理，提取最左侧第一个非空 IP
                            if (real_client_ip.empty()) {
                                auto it_xff = http_req->find("x-forwarded-for");
                                if (it_xff != http_req->end() && !it_xff->value().empty()) {
                                    std::string xff_value = std::string(it_xff->value());
                                    std::string::size_type comma_pos = xff_value.find(',');
                                    
                                    if (comma_pos != std::string::npos) {
                                        real_client_ip = xff_value.substr(0, comma_pos);
                                    } else {
                                        real_client_ip = xff_value;
                                    }
                                }
                            }

                            // 【第三级：物理 Socket 终极兜底】
                            // 如果以上头部全部为空，说明没有任何反向代理，属于客户端直接连接我们的服务器（如本地开发测试）
                            if (real_client_ip.empty()) {
                                boost::system::error_code ec_endpoint;
                                auto ep = boost::beast::get_lowest_layer(ws_).socket().remote_endpoint(ec_endpoint);
                                if (!ec_endpoint) {
                                    real_client_ip = ep.address().to_string();
                                } else {
                                    real_client_ip = "127.0.0.1"; // 极端异常断开时的安全默认值
                                }
                            }

                            // 【第四步：清洗首尾可能残留的空格】
                            real_client_ip.erase(0, real_client_ip.find_first_not_of(" "));
                            real_client_ip.erase(real_client_ip.find_last_not_of(" ") + 1); 

                            // 3. 将解析出来的真实 IP 赋值给你的成员变量（假设名为 client_ip_）
                            client_ip_ = real_client_ip;

                            // 打印日志验证结果（如果你需要的话）
                            LOG_DEBUG("session[%d] Websocket upgrade success. Real Client IP: %s", session_id(), client_ip_.c_str());
                        }

                        // Read a message
                        buffer_.clear();
                        do_read();
                    });
                });
            };

#ifdef ENABLE_WEBSOCKET_SSL
        // Set the timeout.
        boost::beast::get_lowest_layer(ws_).expires_after(std::chrono::seconds(30));
        // Perform the SSL handshake
        ws_.next_layer().async_handshake(boost::asio::ssl::stream_base::server, [this, self, do_http_read](boost::beast::error_code ec) {
            if (ec) {
                LOG_ERR("WebsocketConnection::on_handshake error, errorcode[%d]:%s", ec.value(), ec.message().c_str());
                AsyncClose();
                return;
            }
            
            do_http_read(); 
        });
#else
        do_http_read();
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
            LOG_ERR("WebsocketConnection::on_read received incomplete client package, buff_size: %lu < msg_head_size: %lu", buff_size, sizeof(CMsgHeader));
            return;
        }

        std::vector<unsigned char> byte_array(buff_size);
        boost::asio::buffer_copy(boost::asio::buffer(byte_array), buffer_data);
        buffer_.clear();
        CMsgHeader* msg = reinterpret_cast<CMsgHeader*>(byte_array.data());
        if(byte_array.size() != buff_size){
            AsyncClose();
            LOG_ERR("WebsocketConnection::on_read copy data error, buff_size: %lu != byte_array_size: %lu", buff_size, byte_array.size());
            return;
        }
        CMsgHeaderNtoh(msg);

        if (msg->length > recv_client_message_max_length){
            LOG_ERR("WebsocketConnection::on_read Client msg length %d > %d", msg->length, recv_client_message_max_length);
            AsyncClose();
            return;
        }

        int msg_length = sizeof(*msg) + msg->length;
        if (msg_length < 0 || buff_size != static_cast<size_t>(msg_length)){ // 不够一个完整的包
            LOG_ERR("WebsocketConnection::on_read received incomplete client package, msg_length: %d != buff_size: %lu", msg_length, buff_size);
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
    {
        boost::system::error_code ec;
        deadline_timer_.cancel(ec);
    }
    
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