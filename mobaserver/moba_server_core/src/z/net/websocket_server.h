#ifndef Z_NET_WEBSOCKET_SERVER_H
#define Z_NET_WEBSOCKET_SERVER_H

#include "c_server.h"
#include "websocket_connection.h"

namespace z {
namespace net {

class ICMsgHandler;

class WebsocketServer : public CCServer
{
    WebsocketServer()
#ifdef ENABLE_WEBSOCKET_SSL
        :ssl_ctx_{ boost::asio::ssl::context::tlsv12}
#endif
    {}
public:
    ~WebsocketServer(){}

    bool Init(const std::string& address, const std::string& port, ICMsgHandler* handler);

    virtual std::shared_ptr<IConnection> CreateConnection(boost::asio::ip::tcp::socket&& sock, int32 session_id)
    {
        return std::make_shared<WebsocketConnection>(this, session_id, std::move(sock));
    }

#ifdef ENABLE_WEBSOCKET_SSL
    boost::asio::ssl::context& ssl_ctx() { return ssl_ctx_; }
#endif

private:
#ifdef ENABLE_WEBSOCKET_SSL
    boost::asio::ssl::context ssl_ctx_;
#endif

private:
    DECLARE_SINGLETON(WebsocketServer);
};


} // namespace net
} // namespace z

#define WEBSOCKET_SERVER Singleton<z::net::WebsocketServer>::instance()


#endif // Z_NET_WEBSOCKET_SERVER_H

