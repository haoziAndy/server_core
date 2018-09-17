#include "stdafx.h"
#include "c_connection.h"
#include "msg_header.h"
#include "msg_handler.h"
#include "c_server.h"
#include "session_mgr.h"

namespace z {
namespace net {


CConnection::CConnection( IServer* server, int conn_index )
    : IConnection(server, conn_index)
    , status_(LoginStatus_DEFAULT)
    , open_id_(0)
    , idle_count_(0)
    , msg_count_(0)
{

}

CConnection::~CConnection()
{
}


void CConnection::Start()
{
    ++op_count_;
    auto timeout_sec = CSERVER.login_time_out_sec();
    deadline_timer_.expires_from_now(boost::posix_time::seconds(timeout_sec));
    deadline_timer_.async_wait(
        boost::bind(&CConnection::OnLoginTimeOut, this, boost::asio::placeholders::error));

    IConnection::Start();
}

int32 CConnection::OnRead( char* data, int32 length )
{
    int32 disposed_size = 0;
    while (true)
    {    
        /// @todo decryt

        CMsgHeader* msg = reinterpret_cast<CMsgHeader*>(data + disposed_size);

        int left_length = length - disposed_size;
        if (left_length < static_cast<int>(sizeof(*msg)))
            break;        

        // error if session is waiting for server verify, should not recv more msg
        if (status_ == LoginStatus_ACCOUNT_LOGIN)
        {
            return -1;
        }

        /// 字节序转换
        CMsgHeaderNtoh(msg);

		msg->length -= sizeof(msg->msg_id);//由于客户端现在把消息id的长度也算在包长里面


        // 客户端发给服务器的消息 最大长度限制
        if (msg->length > recv_client_message_max_length)
        {
			LOG_ERR("Client msg length %d > %d", msg->length, recv_client_message_max_length);
            return -1;
        }

        int msg_length = sizeof(*msg) + msg->length;
        if (left_length < msg_length)
        {
            CMsgHeaderHton(msg);
            break;
        }
#if NON_PERSISTANCE_MODE
        // 短连接 
        if (msg->openid != 0)
        {
            // 校验 header 
            if (!SESSION_MGR.IsChecksumValid(msg->openid, msg->checksum, std::string(msg->key, sizeof(msg->key))))
            {
                return -1;
            }
            // 直接跳过SetLoginStatus一步一步检查
            // SetLoginStatus(LoginStatus_PLAY_GAME);
            status_ = LoginStatus_PLAY_GAME;
            // 登录的timer取消, 开始keepalive timer
            StartKeepAliveTimer();
            // 设置相关数据
            session_data_ = SESSION_MGR.GetSessionData(msg->openid);
            set_user_id(session_data_->openid);
            set_server_group_id(session_data_->sid);
        }
#endif
        int h_ret = CSERVER.request_handler()->OnMessage(this, msg);
        // 消息处理 返回-1, 表明应该断开连接
        if (h_ret < 0)
        {
            return -1;
        }

        disposed_size += msg_length;

        // 第一个消息必须是登录消息
        if (status_ == LoginStatus_DEFAULT)
            status_ = LoginStatus_ACCOUNT_LOGIN;
        // 设置idle_count
        idle_count_ = 0;
        ++ msg_count_;
    }
    return disposed_size;
}

void CConnection::OnWrite( int32 length )
{
    for (auto it = send_queue_.begin(); it != send_queue_.end(); ++it)
    {
        auto& buffer = *it;
        const char* data = boost::asio::buffer_cast<const char*>(buffer);
        ZPOOL_FREE(static_cast<void*>(const_cast<char*>(data)));
    }
}

void CConnection::OnLoginTimeOut(const boost::system::error_code& ec)
{
    --op_count_;
    if (!ec)
    {
        // timeout 还处于账户登录, 断开
        if (status_ <= LoginStatus_ACCOUNT_LOGIN)
        {
            LOG_DEBUG("session[%d] OnLoginTimeOut. close.", session_id());
            AsyncClose();
            return;
        }
        else
        {
            idle_count_ = 0;
            msg_count_ = 0;
            StartKeepAliveTimer();
        }
    }

    if (op_count_ == 0)
    {
        AsyncClose();
    }
}

void CConnection::StartKeepAliveTimer()
{
    ++op_count_;
    deadline_timer_.expires_from_now(boost::posix_time::seconds(1));
    deadline_timer_.async_wait(
        boost::bind(&CConnection::OnKeepAliveTimeOut, this, boost::asio::placeholders::error));
}

void CConnection::OnKeepAliveTimeOut( const boost::system::error_code& ec )
{
    --op_count_;
    if (!ec)
    {
#if NON_PERSISTANCE_MODE
        // 如果是短连接, 直接关闭, 未完成的发送会发送完成
        if (session_data_ != nullptr && session_data_->persistence_mode == 0)
        {
            // 短连接 1秒直接断开
            // if (session_data_->last_active_sec < TIME_ENGINE.time_sec64())
            {
                AsyncClose();
                return;
            }
            
        }
#endif
        // 判断连接5分钟没有消息就断开
        auto timeout_sec = CSERVER.keepalive_time_out_sec();
        if (idle_count_ >= timeout_sec)
        {
            LOG_DEBUG("session[%d] OnKeepAliveTimeOut. idle >= %d. close.", session_id(), timeout_sec);
            AsyncClose();
            return;
        }        
        else
        {
            // 判断连接5分钟没有消息就断开
            enum {idle_max_count = 300};
            if (idle_count_ >= idle_max_count)
            {
                LOG_DEBUG("session[%d] OnKeepAliveTimeOut. idle >= %d. close.", session_id(), idle_max_count);
                AsyncClose();
                return;
            }
            // 消息过多, 断开 TODO(zen): 设置 数目
            enum { max_msg_flood = 1024 };
            if (msg_count_ >= max_msg_flood)
            {
                LOG_DEBUG("session[%d] recv too many msgs. msg > %d per sec. close.", session_id(), max_msg_flood);
                AsyncClose();
                return;
            }
        }
        
        ++idle_count_;
        msg_count_ = 0;
        StartKeepAliveTimer();
    }

    if (op_count_ == 0)
    {
        AsyncClose();
    }
}

int CConnection::SetLoginStatus( LoginStatus status )
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

void CConnection::OnClosed()
{
    for (auto it = send_queue_.begin(); it != send_queue_.end(); ++it)
    {
        auto& buffer = *it;
        const char* data = boost::asio::buffer_cast<const char*>(buffer);
        ZPOOL_FREE(static_cast<void*>(const_cast<char*>(data)));
    }
    send_queue_.clear();

    for (auto it = pending_send_queue_.begin(); it != pending_send_queue_.end(); ++it)
    {
        auto& buffer = *it;
        const char* data = boost::asio::buffer_cast<const char*>(buffer);
        ZPOOL_FREE(static_cast<void*>(const_cast<char*>(data)));
    }
    pending_send_queue_.clear();

    CSERVER.request_handler()->OnClientDisconnect(this);
}



} // namespace net
} // namespace z
