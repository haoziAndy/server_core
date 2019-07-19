#ifndef Z_NET_U_CONNECTION_H
#define Z_NET_U_CONNECTION_H

#include "msg_handler.h"

namespace z {
namespace net {

class IUMsgHandler;

// udp c连接类, 
class UConnection
{
    // 8192 - 64
    enum {recv_client_message_max_length = 8128 };
public:
    UConnection(int session_id, const kcp_conv_t _kcp_conv_t);
    virtual ~UConnection();
    
    /// 
    void OnClose();

    int32 OnRead(char* data, int32 length);

    int session_id() const { return session_id_; }
   
    std::string user_id() const { return user_id_; }
    void set_user_id(const std::string &_user_id) {user_id_ = _user_id;}
    
    bool IsForwarding() const
    {
        return status_ > LoginStatus_PLAYER_LOGIN && status_ < LoginStatus_CLOSING;
    }
    int32 GetLoginStatus() const {return status_;}
    int SetLoginStatus(LoginStatus status);

	kcp_conv_t kcp_conv() const { return kcp_conv_t_; }

protected:
    boost::asio::deadline_timer deadline_timer_; 

    int32 session_id_;
    std::string user_id_;

	kcp_conv_t kcp_conv_t_;

    LoginStatus status_;

    DISALLOW_COPY_AND_ASSIGN(UConnection);
};


} // namespace net
} // namespace z

#endif  // Z_NET_U_CONNECTION_H
