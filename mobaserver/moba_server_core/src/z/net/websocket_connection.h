#ifndef Z_NET_WEBSOCKET_CONNECTION_H
#define Z_NET_WEBSOCKET_CONNECTION_H

#include "i_connection.h"

namespace z {
namespace net {

class WebsocketServer;

class WebsocketConnection : public IConnection
{    
    enum {recv_client_message_max_length = 8192 };
public:
    WebsocketConnection(WebsocketServer* server, int conn_index, boost::asio::ip::tcp::socket&& sock);
    virtual ~WebsocketConnection();

    virtual void Start();
    virtual boost::asio::ip::tcp::socket& socket() { return boost::beast::get_lowest_layer(ws_).socket();}

    virtual void StartWrite();

    virtual void Close();

    std::shared_ptr<WebsocketConnection> shared_from_this() {
        return std::dynamic_pointer_cast<WebsocketConnection>(IConnection::shared_from_this());
    };

private:
    void StartKeepAliveTimer();
    void do_read();
    virtual int32 OnRead(char* data, int32 length) { return 0; };

private:

#ifdef ENABLE_WEBSOCKET_SSL
    boost::beast::websocket::stream<boost::beast::ssl_stream<boost::beast::tcp_stream>> ws_;
#else
    boost::beast::websocket::stream<boost::beast::tcp_stream> ws_;
#endif
    boost::beast::flat_buffer buffer_;

    int32 idle_count_;                  // 防止发呆, 每秒加1, 到某值就判定断开, 有消息读取置零
    int32 msg_count_;                   // 消息计数, 防止过多消息
};


} // namespace net
} // namespace z

#endif // Z_NET_WEBSOCKET_CONNECTION_H
