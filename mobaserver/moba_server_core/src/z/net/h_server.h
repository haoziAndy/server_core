#ifndef Z_NET_H_SERVER_H
#define Z_NET_H_SERVER_H

#include "i_server.h"
#include "h_connection.h"

namespace z
{
namespace net
{

class IHttpRequestHandler;

class HServer : public IServer
{
    HServer()
    {}
public:
    ~HServer()
    {}
    // 
    bool Init(const std::string& address, const std::string& port, z::net::IHttpRequestHandler* handler, int32 timeout_sec = 2);

    virtual IConnection* CreateConnection(int32 session_id)
    {
        return ZPOOL_NEW(HConnection, this, session_id, timeout_sec_);
    }

    void SendToSession(int32 session_id, const std::string& data);

    z::net::IHttpRequestHandler* request_handler() const {return request_handler_;}
    void set_request_handler(z::net::IHttpRequestHandler* handler) 
    {
        request_handler_ = handler;
    }
private:
    z::net::IHttpRequestHandler* request_handler_;
    int32 timeout_sec_;

private:
    DECLARE_SINGLETON(HServer);
};


} // namespace net
} // namespace z

#define HTTP_SERVER Singleton<z::net::HServer>::instance()


#endif // Z_NET_H_SERVER_H

