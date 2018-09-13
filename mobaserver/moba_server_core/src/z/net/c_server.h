#ifndef Z_NET_CSERVER_H
#define Z_NET_CSERVER_H

#include "i_server.h"
#include "c_connection.h"

namespace z {
namespace net {

class ICMsgHandler;
struct SMsgHeader;

class CCServer : public IServer
{
    CCServer()
        :request_handler_(nullptr)
        , login_time_out_sec_(0)
        , keepalive_time_out_sec_(0)
    {}
public:
    ~CCServer()
    {}

    bool Init(const std::string& address, const std::string& port, ICMsgHandler* handler);

    virtual IConnection* CreateConnection(int32 session_id)
    {
        return ZPOOL_NEW(CConnection, this, session_id);
    }

    z::net::ICMsgHandler* request_handler() const {return request_handler_;}

    int32 login_time_out_sec() const {return login_time_out_sec_;}
    void set_login_time_out_sec(int32 timeout_sec) {login_time_out_sec_ = timeout_sec;}

    int32 keepalive_time_out_sec() const {return keepalive_time_out_sec_;}
    void set_keepalive_time_out_sec(int32 time_out_sec) { keepalive_time_out_sec_ = time_out_sec; }

    void set_msg_names();

    void SendToSession(int session_id, const std::string & user_id, SMsgHeader* msg);
private:
    void SendToSession(int session_id, const std::string & user_id, CMsgHeader* msg);

private:
    z::net::ICMsgHandler* request_handler_;
    
    int32 login_time_out_sec_;
    int32 keepalive_time_out_sec_;

private:
    DECLARE_SINGLETON(CCServer);
};


} // namespace net
} // namespace z

#define CSERVER Singleton<z::net::CCServer>::instance()


#endif // Z_NET_CSERVER_H

