#ifndef Z_NET_U_CONNECTION_H
#define Z_NET_U_CONNECTION_H

#include "msg_handler.h"
typedef uint32_t kcp_conv_t;

namespace z {
namespace net {

class IUMsgHandler;

// udp c连接类, 
class UConnection
{
    // 8192 - 64
    enum {recv_client_message_max_length = 8128 };
public:
    UConnection(int session_id, IUMsgHandler* msg_handler);
    virtual ~UConnection();
    
    /// 
    void OnClose();

    int32 OnRead(char* data, int32 length);

    int session_id() const { return session_id_; }
   
    uint64 user_id() const { return user_id_; }
    void set_user_id(int64 _user_id) {user_id_ = _user_id;}
    
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

    void SecondTimerHandler(const boost::system::error_code& ec);

	void UConnection::SetUdpRemoteEndpoint(const boost::asio::ip::udp::endpoint& udp_remote_endpoint);

	int UConnection::udp_output(const char *buf, int len, ikcpcb *kcp, void *user);
private:
	void UConnection::InitKcp(const kcp_conv_t& conv);
protected:
    boost::asio::deadline_timer deadline_timer_; 

    int32 session_id_;
    int64 user_id_;
    int64 open_id_;
    int32 sid_;

    IUMsgHandler* msg_handler_;

    LoginStatus status_;

    DISALLOW_COPY_AND_ASSIGN(UConnection);
private:
	kcp_conv_t conv_;
	IKCPCB* p_kcp_; // --own
	uint32_t last_packet_recv_time_;
	boost::asio::ip::udp::endpoint udp_remote_endpoint_;
};


} // namespace net
} // namespace z

#endif  // Z_NET_U_CONNECTION_H
