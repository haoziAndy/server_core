#ifndef Z_NET_C_CONNECTION_H
#define Z_NET_C_CONNECTION_H

#include "i_connection.h"

namespace z {
namespace net {

class CConnection : public IConnection
{    
    enum {recv_client_message_max_length = 8192 };
public:
    CConnection(IServer* server, boost::asio::ip::tcp::socket&& sock, int conn_index);
    virtual ~CConnection();

    virtual void Start();

    virtual int32 OnRead(char* data, int32 length);

    void StartKeepAliveTimer();

    // on closed release memory
    virtual void OnClosed();

private:
    int32 idle_count_;                  // 防止发呆, 每秒加1, 到某值就判定断开, 有消息读取置零
    int32 msg_count_;                   // 消息计数, 防止过多消息

};


} // namespace net
} // namespace z

#endif // Z_NET_C_CONNECTION_H
