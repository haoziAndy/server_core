#include "stdafx.h"
#include "u_connection.h"
#include "msg_header.h"
#include "z_server.h"
#include "udp_server.h"

namespace z {
namespace net {

UConnection::UConnection( int session_id,const kcp_conv_t _kcp_conv_t):
	deadline_timer_(TIME_ENGINE)
    , session_id_(session_id)
    , user_id_("")
    , kcp_conv_t_(_kcp_conv_t) 
	, status_(LoginStatus_DEFAULT)
{
};

UConnection::~UConnection()
{
    boost::system::error_code ignored_ec;
    deadline_timer_.cancel(ignored_ec);
}

void UConnection::OnClose()
{
    boost::system::error_code ignored_ec;
    deadline_timer_.cancel(ignored_ec);
}

int32 UConnection::OnRead( char* data, int32 length )
{
    int32 handled_size = 0;
    while (handled_size < length)
    {
        CMsgHeader* msg = reinterpret_cast<CMsgHeader*>(data + handled_size);

        int left_length = length - handled_size;
        if (left_length < static_cast<int>(sizeof(*msg)))
            break;  
        /// 字节序转换
        CMsgHeaderNtoh(msg);

        // error if session is waiting for server verify, should not recv more msg
        if (status_ == LoginStatus_ACCOUNT_LOGIN)
        {
            return -1;
        }

        // 客户端发给服务器的消息 最大长度限制
        if (msg->length > recv_client_message_max_length)
        {
            return -1;
        }

        int msg_length = sizeof(*msg) + msg->length;
        if (left_length < msg_length)
        {
            CMsgHeaderHton(msg);
            break;
        }

        int h_ret = UDPSERVER.request_handler()->OnMessage(this, msg);
        // 消息处理 返回-1, 表明应该断开连接
        if (h_ret < 0)
            return -1;

        handled_size += msg_length;

        // 第一个消息必须是登录消息
        if (status_ == LoginStatus_DEFAULT)
            status_ = LoginStatus_ACCOUNT_LOGIN;
    }
    return handled_size == length ? 0 : -1;
}

int UConnection::SetLoginStatus( LoginStatus status )
{
    // 玩家可以跳过
    if (static_cast<int>(status_) + 1 != static_cast<int>(status) 
        && !(status_ == LoginStatus_PLAYER_CREATE && status == LoginStatus_PLAY_GAME))
    {
        LOG_ERR("Wrong status step != 1 and not player_login");
        return -1;
    }
    if (status >= LoginStatus_CLOSING)
    {
        LOG_ERR("closing status could Only be set by AsyncClose() ");
        return -1;
    }
    status_ = status;
    return 0;
}

} // namespace net
} // namespace z

