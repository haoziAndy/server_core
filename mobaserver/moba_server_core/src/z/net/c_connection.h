#ifndef Z_NET_C_CONNECTION_H
#define Z_NET_C_CONNECTION_H

#include "i_connection.h"
#include "msg_handler.h"

namespace z {
namespace net {

struct CMsgHeader;
struct SessionData;

class CConnection : public IConnection
{    
    enum {recv_client_message_max_length = 8192 };
public:
    CConnection(IServer* server, int conn_index);
    virtual ~CConnection();

    virtual void Start();

    virtual int32 OnRead(char* data, int32 length);

    virtual void OnWrite(int32 length);

    /// on login timeout
    void OnLoginTimeOut(const boost::system::error_code& ec);
    /// 
    void StartKeepAliveTimer();
    /// on keep alive timeout
    void OnKeepAliveTimeOut(const boost::system::error_code& ec);

    void set_account_id(const std::string &account_id) { account_id_ = account_id;}
    std::string account_id() const {return account_id_;}

    /*void set_server_group_id(int server_group_id) {server_group_id_ = server_group_id;}
    int server_group_id() const {return server_group_id_;}*/

    void SetSessionData(boost::shared_ptr<SessionData>& data)
    {
        session_data_ = data;
    }

    int SetLoginStatus(LoginStatus status);
    enum LoginStatus GetLoginStatus() const
    {
        return status_;
    };

    bool IsForwarding() const
    {
        return status_ > LoginStatus_PLAYER_LOGIN && status_ < LoginStatus_CLOSING;
    }
    // on closed release memory
    virtual void OnClosed();

private:
    LoginStatus status_;
    std::string account_id_;
    //int32 server_group_id_;

    int32 idle_count_;                  // 防止发呆, 每秒加1, 到某值就判定断开, 有消息读取置零
    int32 msg_count_;                   // 消息计数, 防止过多消息

    // session data, 长连接 短连接 兼容用
    boost::shared_ptr<SessionData> session_data_;
};


} // namespace net
} // namespace z

#endif // Z_NET_C_CONNECTION_H
