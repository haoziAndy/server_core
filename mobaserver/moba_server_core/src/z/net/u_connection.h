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
    UConnection(int session_id, const kcp_conv_t _kcp_conv_t, IUMsgHandler* msg_handler);
    virtual ~UConnection();
    
    /// 
    void OnClose();

    int32 OnRead(char* data, int32 length);

    int session_id() const { return session_id_; }
   
    std::string user_id() const { return user_id_; }
    void set_user_id(const std::string &_user_id) {user_id_ = _user_id;}
    
    void set_open_id(int64 _open_id) {open_id_ = _open_id;}
    int64 open_id() const {return open_id_;}
    void set_sid(int32 _sid) {sid_ = _sid;}
    int32 sid() const {return sid_;}
    
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
    int64 open_id_;
    int32 sid_;

    IUMsgHandler* msg_handler_;

    LoginStatus status_;

	kcp_conv_t kcp_conv_t_;

    DISALLOW_COPY_AND_ASSIGN(UConnection);
};


} // namespace net
} // namespace z

#endif  // Z_NET_U_CONNECTION_H
