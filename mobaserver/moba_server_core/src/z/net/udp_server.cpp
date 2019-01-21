#include "stdafx.h"
#include "udp_server.h"
#include "msg_header.h"
#include "msg_handler.h"
#include "z_server.h"
#include "u_connection.h"

namespace z {
namespace net {


UdpServer::UdpServer():
	udp_socket_(TIME_ENGINE)
    ,request_handler_(nullptr)
    , signals_(TIME_ENGINE)
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
	boost::asio::ip::udp::resolver resolver(TIME_ENGINE);
	boost::asio::ip::udp::resolver::query query(addr, port);

	boost::system::error_code ec;
	auto it = resolver.resolve(query, ec);
	if (ec)
	{
		LOG_ERR("UdpServer::Init failed. Error[%d]: %s", ec.value(), ec.message().c_str());
		return false;
	}

	boost::asio::ip::udp::endpoint _endpoint = *it;
	this->udp_socket_.bind(_endpoint,ec);
	if (ec)
	{
		LOG_ERR("UdpServer::Init failed. Error[%d]: %s", ec.value(), ec.message().c_str());
		return false;
	}

    request_handler_ = handler;

    InitPollTimer();

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
    // alloc avail session id
    static int s_session_id = 0;
    auto session_id = ((++s_session_id) << 1) | 0x0;
    for (int i=0; session_conn_.find(session_id) != session_conn_.end() && i<100; ++i)
        ++ session_id;
    
    if (session_conn_.find(session_id) != session_conn_.end())
    {
        LOG_ERR("failed alloc session_id");
        return -1;
    }
    auto conn = ZPOOL_NEW_SHARED(z::net::UConnection, session_id, conn_guid, request_handler_);
    session_conn_[session_id] = conn;
	
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
	this->udp_socket_.cancel();
	this->udp_socket_.close();
	is_server_shutdown_ = true;
}

void UdpServer::Destroy()
{

}


void UdpServer::HandleUdpReceiveFrom(const boost::system::error_code& error, size_t bytes_recvd)
{
	if (!error && bytes_recvd > 0)
	{/*
		if (asio_kcp::is_connect_packet(udp_data_, bytes_recvd))
		{
			//handle_connect_packet();
			goto END;
		}
		*/
		//handle_kcp_packet(bytes_recvd);
	}
	else
	{
		LOG_ERR("\nhandle_udp_receive_from error end! error: %s, bytes_recvd: %ld\n", error.message().c_str(), bytes_recvd);
	}

END:
	HookUdpAsyncReceive();
}

void UdpServer::HookUdpAsyncReceive(void)
{
	if (is_server_shutdown_)
		return;
	udp_socket_.async_receive_from(
		boost::asio::buffer(udp_data_, sizeof(udp_data_)), udp_remote_endpoint_,
		boost::bind(&UdpServer::HandleUdpReceiveFrom, this,
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred));
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

