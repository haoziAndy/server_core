#include "stdafx.h"
#include "udp_server.h"
#include "msg_header.h"
#include "msg_handler.h"
#include "z_server.h"
#include "u_connection.h"
#include "asio_kcp/server.hpp"

namespace z {
namespace net {


UdpServer::UdpServer():
    request_handler_(nullptr)
    , signals_(TIME_ENGINE)
	,kcp_server_(TIME_ENGINE)
    , is_server_shutdown_(false)
	, login_time_out_sec_(1)
{}

bool UdpServer::Init(const std::string& addr, const std::string& port, IUMsgHandler* handler)
{
    if (handler == nullptr)
    {
        LOG_ERR("icmsg handler == nullptr");
        return false;
    }

	if (!kcp_server_.init(addr, port))
	{
		LOG_ERR("Failed to init kcp_server_");
		return false;
	}

    request_handler_ = handler;


    signals_.add(SIGINT);
    signals_.add(SIGTERM);
#if defined(SIGQUIT)
    signals_.add(SIGQUIT);
#endif // defined(SIGQUIT)
    signals_.async_wait(boost::bind(&UdpServer::Stop, this));

    return true;
}

void UdpServer::SendToSession( int session_id, const std::string & user_id, SMsgHeader* msg )
{
    if (msg->length > 0xffff)
    {
        LOG_ERR("client msg length > 0xffff, id %d", msg->msg_id);
        return;
    }
    z::net::CMsgHeader* cmsg;
    int buff_length = msg->length + sizeof(*cmsg);
    unsigned char* buff = reinterpret_cast<unsigned char*> (ZPOOL_MALLOC(buff_length + 1));
    cmsg = reinterpret_cast<z::net::CMsgHeader*>(buff);
    cmsg->length = msg->length;
    cmsg->msg_id = msg->msg_id;
    memcpy(cmsg+1, msg+1, msg->length);

    SendToSession(session_id, user_id, cmsg);
}

void UdpServer::SendToSession( int session_id,const std::string &user_id, CMsgHeader* msg )
{
    auto it = session_conn_.find(session_id);
    if (it == session_conn_.end())
    {
        LOG_DEBUG("Stop SendMsg[%d] to session[%d] user %s: not found client session",
            msg->msg_id, session_id, user_id.c_str());
        ZPOOL_FREE(msg);
        return;
    }
    auto& conn = it->second;
    if (conn.get() == nullptr)
    {
        LOG_ERR("NULL connection by player %s", user_id.c_str());
        ZPOOL_FREE(msg);
        return;
    }
    if (conn->user_id() != user_id)
    {
        LOG_ERR("Stop SendMsg[%d] to session[%d]: user_id mismatched. need %s, get %s",
            msg->msg_id, session_id, user_id.c_str(), conn->user_id().c_str());
        ZPOOL_FREE(msg);
        return;
    }
    auto length = msg->length + sizeof(*msg) + 1;

    CMsgHeaderHton(msg);

    char* buff = reinterpret_cast<char*>(msg) - 1;
	kcp_server_.send_msg(conn->kcp_conv(), buff, length);

   // server_->Send(buff, length, HIGH_PRIORITY, RELIABLE_ORDERED, 0, conn->raknet_guid(), false);
    ZPOOL_FREE(buff);
}

int32 UdpServer::AddNewConnection( const kcp_conv_t _kcp_conv_t)
{
	// alloc avail session id
	static int s_session_id = 0;
	auto session_id = ((++s_session_id) << 1) | 0x0;
	for (int i = 0; session_conn_.find(session_id) != session_conn_.end() && i < 100; ++i)
		++session_id;

	if (session_conn_.find(session_id) != session_conn_.end())
	{
		LOG_FATAL("failed alloc session_id");
		kcp_server_.force_disconnect(_kcp_conv_t);
		return -1;
	}
	auto conn = ZPOOL_NEW_SHARED(z::net::UConnection, session_id, _kcp_conv_t);
	session_conn_[session_id] = conn;

    return session_id;
}

int32 UdpServer::RemoveConnection(const int32 session_id)
{
     auto it = session_conn_.find(session_id);
    if (it != session_conn_.end())
    {
        auto& conn = it->second;

        request_handler()->OnClientDisconnect(conn.get());
		kcp_server_.force_disconnect(conn->kcp_conv());
        conn->OnClose();
		session_conn_.erase(it);
    }
    return 0;
}

void UdpServer::Run()
{
    ZSERVER.Run();
}

void UdpServer::Stop()
{
	is_server_shutdown_ = true;
	kcp_server_.stop();
}

void UdpServer::Destroy()
{

}

boost::shared_ptr<UConnection> UdpServer::GetConnection( int32 session_id ) const
{
    auto it = session_conn_.find(session_id);
    if (it != session_conn_.end())
    {
        return it->second;
    }
    else
    {
        static boost::shared_ptr<UConnection> empty_ptr;
        return empty_ptr;
    }
}

void UdpServer::CloseConnection( int32 session_id )
{
    auto it = session_conn_.find(session_id);
    if (it != session_conn_.end())
    {
        auto& conn = it->second;
        if (conn.get() != nullptr)
        {
            RemoveConnection(session_id);
        }
    }
}

} // namespace net
} // namespace z

