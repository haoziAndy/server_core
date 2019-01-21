#include "stdafx.h"
#include "udp_server.h"
#include "msg_header.h"
#include "msg_handler.h"
#include "z_server.h"
#include "u_connection.h"
#include "asio_kcp/server.hpp"

namespace z {
namespace net {


UdpServer::UdpServer(const std::string& address, const std::string& port):
    request_handler_(nullptr)
    , signals_(TIME_ENGINE)
	,kcp_server_(TIME_ENGINE, address, port)
    , is_server_shutdown_(false)
	, poll_timer_(TIME_ENGINE)
{}

bool UdpServer::Init(const std::string& addr, const std::string& port,const int32 max_incomming_conn, IUMsgHandler* handler )
{
    if (handler == nullptr)
    {
        LOG_ERR("icmsg handler == nullptr");
        return false;
    }
    if (max_incomming_conn <= 0)
    {
        LOG_ERR("max_incomming_conn %d", max_incomming_conn);
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

void UdpServer::SendToSession( int session_id, uint64 user_id, SMsgHeader* msg )
{
   /* if (msg->length > 0xffff)
    {
        LOG_ERR("client msg length > 0xffff, id %d", msg->msg_id);
        return;
    }
    z::net::CMsgHeader* cmsg;
    int buff_length = msg->length + sizeof(*cmsg);
    unsigned char* buff = reinterpret_cast<unsigned char*> (ZPOOL_MALLOC(buff_length + 1));
    buff[0] = ID_USER_PACKET_ENUM;
    cmsg = reinterpret_cast<z::net::CMsgHeader*>(buff + 1);
    cmsg->length = msg->length;
    cmsg->msg_id = msg->msg_id;
//    cmsg->send_tick = msg->send_tick;
    memcpy(cmsg+1, msg+1, msg->length);

    SendToSession(session_id, user_id, cmsg);*/
}

void UdpServer::SendToSession( int session_id, uint64 user_id, CMsgHeader* msg )
{
    auto it = session_conn_.find(session_id);
    if (it == session_conn_.end())
    {
        LOG_DEBUG("Stop SendMsg[%d] to session[%d] user %" PRId64 ": not found client session",
            msg->msg_id, session_id, user_id);
        ZPOOL_FREE(msg);
        return;
    }
    auto& conn = it->second;
    if (conn.get() == nullptr)
    {
        LOG_ERR("NULL connection by player %" PRIu64, user_id);
        ZPOOL_FREE(msg);
        return;
    }
    if (conn->user_id() != user_id)
    {
        LOG_ERR("Stop SendMsg[%d] to session[%d]: user_id mismatched. need %" PRId64 ", get %" PRId64,
            msg->msg_id, session_id, user_id, conn->user_id());
        ZPOOL_FREE(msg);
        return;
    }
    auto length = msg->length + sizeof(*msg) + 1;

    CMsgHeaderHton(msg);

    char* buff = reinterpret_cast<char*>(msg) - 1;
   // server_->Send(buff, length, HIGH_PRIORITY, RELIABLE_ORDERED, 0, conn->raknet_guid(), false);
    ZPOOL_FREE(buff);
}

int32 UdpServer::AddNewConnection(  )
{

    return 0;
}

int32 UdpServer::RemoveConnection( )
{
   /* auto it = guid_conn_.find(conn_guid);
    if (it != guid_conn_.end())
    {
        auto& conn = it->second;

        request_handler()->OnClientDisconnect(conn.get());
        server_->CloseConnection(conn->raknet_guid(), true);
        conn->OnClose();

        auto s_it = session_conn_.find(conn->session_id());
        if (s_it != session_conn_.end())
            session_conn_.erase(s_it);
        guid_conn_.erase(it);
    }*/
    return 0;
}

void UdpServer::Run()
{
    ZSERVER.Run();
}

void UdpServer::Stop()
{
	is_server_shutdown_ = true;
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
    //        RemoveConnection(conn->raknet_guid());
        }
    }
}

void UdpServer::ExecuteUConnTimerFunc( UConnection* conn ) const
{
    uconn_timer_func_(conn);
}


} // namespace net
} // namespace z

