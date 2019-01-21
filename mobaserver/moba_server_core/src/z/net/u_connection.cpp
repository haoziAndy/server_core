#include "stdafx.h"
#include "u_connection.h"
#include "msg_header.h"
#include "z_server.h"
#include "udp_server.h"

namespace z {
namespace net {

UConnection::UConnection( int session_id, IUMsgHandler* msg_handler, const kcp_conv_t& conv):
	deadline_timer_(TIME_ENGINE)
    , session_id_(session_id)
    , user_id_(0)
    , msg_handler_(msg_handler)
    , status_(LoginStatus_DEFAULT)
	,conv_(0)
    ,p_kcp_(nullptr)
    ,last_packet_recv_time_(0)
{
    BOOST_ASSERT(msg_handler != nullptr);
    deadline_timer_.expires_from_now(boost::posix_time::seconds(5));
    deadline_timer_.async_wait(boost::bind(&UConnection::SecondTimerHandler, this, boost::asio::placeholders::error));
	this->InitKcp(conv);
};

UConnection::~UConnection()
{
    boost::system::error_code ignored_ec;
    deadline_timer_.cancel(ignored_ec);
	ikcp_release(p_kcp_);
	p_kcp_ = nullptr;
	conv_ = 0;
}


void UConnection::SetUdpRemoteEndpoint(const boost::asio::ip::udp::endpoint& udp_remote_endpoint)
{
	udp_remote_endpoint_ = udp_remote_endpoint;
}

void UConnection::InitKcp(const kcp_conv_t& conv)
{
	conv_ = conv;
	p_kcp_ = ikcp_create(conv, (void*)this);
	//p_kcp_->output = &connection::udp_output;

	// 启动快速模式
	// 第二个参数 nodelay-启用以后若干常规加速将启动
	// 第三个参数 interval为内部处理时钟，默认设置为 10ms
	// 第四个参数 resend为快速重传指标，设置为2
	// 第五个参数 为是否禁用常规流控，这里禁止
	//ikcp_nodelay(p_kcp_, 1, 10, 2, 1);
	ikcp_nodelay(p_kcp_, 1, 5, 1, 1); // 设置成1次ACK跨越直接重传, 这样反应速度会更快. 内部时钟5毫秒.
}

// 发送一个 udp包
int UConnection::udp_output(const char *buf, int len, ikcpcb *kcp, void *user)
{
	//((UConnection*)user)->send_udp_package(buf, len);
	return 0;
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

        int h_ret = msg_handler_->OnMessage(this, msg);
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

void UConnection::SecondTimerHandler( const boost::system::error_code& ec )
{
    if (!ec)
    {
        deadline_timer_.expires_from_now(boost::posix_time::seconds(5));
        deadline_timer_.async_wait(boost::bind(&UConnection::SecondTimerHandler, this, boost::asio::placeholders::error));

        UDPSERVER.ExecuteUConnTimerFunc(this);
    }
    else
    {
        if (ec.value() != boost::asio::error::operation_aborted)
        {
            LOG_ERR("timer ec %d: %s", ec.value(), ec.message().c_str());
        }
    }
}



} // namespace net
} // namespace z

