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

    boost::asio::dispatch(ws_.get_executor(), [this, self = shared_from_this()]() mutable {
        auto do_http_read = [this, self=std::move(self)]() mutable {
            // 设置读取 HTTP Upgrade 请求的临时超时（防止恶意死连接占用）
            boost::beast::get_lowest_layer(ws_).expires_after(std::chrono::seconds(15));

            // 【Boost 1.73 核心适配点】：WSS 和 WS 下统一安全地传入各自对应的紧邻下一层（Next Layer）
            auto& next_io_layer = ws_.next_layer(); 

            // 异步读取 HTTP 请求
            boost::beast::http::async_read(
                next_io_layer, 
                buffer_, 
                http_req_, 
                [this, self=std::move(self)](boost::beast::error_code ec, std::size_t) mutable {
                    if (ec) {
                        LOG_ERR("session[%d] HTTP read error: %s", session_id(), ec.message().c_str());
                        AsyncClose();
                        return;
                    }

                    // 强校验：如果不是标准的 WebSocket 升级请求，直接拒绝
                    if (!boost::beast::websocket::is_upgrade(http_req_)) {
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
                    ws_.async_accept(http_req_, [this, self=std::move(self)](boost::beast::error_code ec) {
                        if (ec) {
                            if (ec != boost::beast::websocket::error::closed) {
                                LOG_ERR("WebsocketConnection::on_accept error, errorcode[%d]:%s", ec.value(), ec.message().c_str());
                            }

                            AsyncClose();
                            return;
                        }

                        {
                            std::string real_client_ip;
                            boost::beast::string_view raw_ip_view;

                            // 【第一级：X-Forwarded-For】
                            auto it_xff = http_req_.find("x-forwarded-for");
                            if (it_xff != http_req_.end() && !it_xff->value().empty()) {
                                boost::beast::string_view xff_view = it_xff->value();
                                auto comma_pos = xff_view.find(',');

                                if (comma_pos != boost::beast::string_view::npos) {
                                    raw_ip_view = xff_view.substr(0, comma_pos);
                                } else {
                                    raw_ip_view = xff_view;
                                }
                            }

                            // 【第二级：X-Real-IP】
                            if (raw_ip_view.empty()) {
                                auto it_real = http_req_.find("x-real-ip");
                                if (it_real != http_req_.end() && !it_real->value().empty()) {
                                    raw_ip_view = it_real->value();
                                }
                            }

                            // 【标准化清洗】：在视图（View）层面上进行双端去空格，真正做到零内存开销
                            if (!raw_ip_view.empty()) {
                                auto first_not_space = raw_ip_view.find_first_not_of(' ');
                                auto last_not_space = raw_ip_view.find_last_not_of(' ');

                                if (first_not_space != boost::beast::string_view::npos && last_not_space != boost::beast::string_view::npos) {
                                    raw_ip_view = raw_ip_view.substr(first_not_space, last_not_space - first_not_space + 1);
                                    // 此时 raw_ip_view 已经是绝对干净的 IP 视图，安全执行唯一一次 assign
                                    real_client_ip.assign(raw_ip_view.data(), raw_ip_view.size());
                                }
                            }

                            // 【第三级：物理 Socket 终极兜底】
                            if (real_client_ip.empty()) {
                                boost::system::error_code ec_endpoint;
                                auto ep = boost::beast::get_lowest_layer(ws_).socket().remote_endpoint(ec_endpoint);
                                if (!ec_endpoint) {
                                    real_client_ip = ep.address().to_string();
                                }
                            }

                            // 如果依然解析失败（极端情况），安全默认值
                            if (real_client_ip.empty()) {
                                real_client_ip = "127.0.0.1";
                            }

                            //【跨平台/IPv6 额外防御】：拦截由于底层双栈或特定环境可能混入的 IPv6 区域识别符（如 fe80::1%eth0）
                            // 因为 % 绝不是合法 IP 字符，必须在校验前裁切，避免 make_address 抛错
                            auto percent_pos = real_client_ip.find('%');
                            if (percent_pos != std::string::npos) {
                                real_client_ip.resize(percent_pos);
                            }

                            // 验证 IP 格式是否合法（此时 real_client_ip 已经非常干净）
                            boost::system::error_code ec_parse;
                            boost::asio::ip::make_address(real_client_ip, ec_parse);
                            if (ec_parse) {
                                LOG_ERR("session[%d] Invalid IP format from headers: [%s]. Fallback to remote endpoint.", session_id(), real_client_ip.c_str());
                                boost::system::error_code ec_endpoint;
                                auto ep = boost::beast::get_lowest_layer(ws_).socket().remote_endpoint(ec_endpoint);
                                if (!ec_endpoint) {
                                    real_client_ip = ep.address().to_string();
                                    // 兜底获取的物理 IP 如果包含 % 也做一次裁剪
                                    auto pct = real_client_ip.find('%');
                                    if (pct != std::string::npos) {
                                        real_client_ip.resize(pct);
                                    }
                                } else {
                                    real_client_ip = "127.0.0.1";
                                }
                            }

                            // 完美写入成员变量
                            client_ip_ = std::move(real_client_ip);
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
        ws_.next_layer().async_handshake(boost::asio::ssl::stream_base::server, [this, self=std::move(self), do_http_read=std::move(do_http_read)](boost::beast::error_code ec) mutable  {
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
            LOG_ERR("WebsocketConnection::on_read received incomplete client package, buff_size: %llu < msg_head_size: %llu", (unsigned long long)buff_size, (unsigned long long)sizeof(CMsgHeader));
            return;
        }

        std::vector<unsigned char> byte_array(buff_size);
        boost::asio::buffer_copy(boost::asio::buffer(byte_array), buffer_data);
        buffer_.clear();
        CMsgHeader* msg = reinterpret_cast<CMsgHeader*>(byte_array.data());
        if(byte_array.size() != buff_size){
            AsyncClose();
            LOG_ERR("WebsocketConnection::on_read copy data error, buff_size: %llu != byte_array_size: %llu", (unsigned long long)buff_size, (unsigned long long)byte_array.size());
            return;
        }
        CMsgHeaderNtoh(msg);

        if (msg->length > recv_client_message_max_length){
            LOG_ERR("WebsocketConnection::on_read Client msg length %d > %d", msg->length, recv_client_message_max_length);
            AsyncClose();
            return;
        }

        auto msg_length = sizeof(*msg) + msg->length;
        if (msg_length < 0 || buff_size != static_cast<size_t>(msg_length)){ // 不够一个完整的包
            LOG_ERR("WebsocketConnection::on_read received incomplete client package, msg_length: %llu != buff_size: %llu", (unsigned long long)msg_length, (unsigned long long)buff_size);
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
                LOG_ERR("async_write error, errorcode[%d]:%s", ec.value(), ec.message().c_str());
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