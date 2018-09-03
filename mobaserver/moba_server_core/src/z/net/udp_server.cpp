#include "stdafx.h"
#include "udp_server.h"
#include "msg_header.h"
#include "msg_handler.h"
#include "z_server.h"
#include "u_connection.h"
#include <raknet/MessageIdentifiers.h>
#include <raknet/RakPeerInterface.h>
#include <raknet/RakNetStatistics.h>
#include <raknet/RakNetTypes.h>

namespace z {
namespace net {


UdpServer::UdpServer()
    : server_ (nullptr)
    , request_handler_(nullptr)
    , signals_(TIME_ENGINE)
    , is_server_shutdown_(false)
    , poll_timer_(TIME_ENGINE)    
{}

bool UdpServer::Init( const int32 port, const int32 max_incomming_conn, const std::string& password, IUMsgHandler* handler )
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

    server_ = RakNet::RakPeerInterface::GetInstance();
    server_->SetIncomingPassword(password.c_str(), password.size());
    server_->SetTimeoutTime(3000, RakNet::UNASSIGNED_SYSTEM_ADDRESS);

    RakNet::SocketDescriptor socket_descriptor[2];
    socket_descriptor[0].port = port;
    socket_descriptor[0].socketFamily = AF_INET;
    socket_descriptor[1].port = port;
    socket_descriptor[1].socketFamily = AF_INET6; // Test out IPV6
    bool b = (server_->Startup(max_incomming_conn, socket_descriptor, 1) == RakNet::RAKNET_STARTED);
    if (!b)
    {
        LOG_ERR("failed to start server on port %d max_conn %d", port, max_incomming_conn);
        return false;
    }
    server_->SetMaximumIncomingConnections(max_incomming_conn);
    server_->SetOccasionalPing(true);

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
    if (msg->length > 0xffff)
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

    SendToSession(session_id, user_id, cmsg);
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
    server_->Send(buff, length, HIGH_PRIORITY, RELIABLE_ORDERED, 0, conn->raknet_guid(), false);
    ZPOOL_FREE(buff);
}

int32 UdpServer::AddNewConnection( const struct RakNet::RakNetGUID& conn_guid )
{
    auto it = guid_conn_.find(conn_guid);
    if (it != guid_conn_.end())
    {
        LOG_ERR("guid already exist. %" PRIu64, conn_guid.g);
        return -1;
    }
    
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
    guid_conn_[conn_guid] = conn;

    return 0;
}

int32 UdpServer::RemoveConnection( const struct RakNet::RakNetGUID& conn_guid )
{
    auto it = guid_conn_.find(conn_guid);
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
    }
    return 0;
}

void UdpServer::Run()
{
    ZSERVER.Run();
}

void UdpServer::Stop()
{

}

void UdpServer::Destroy()
{

}

int32 UdpServer::Poll()
{
    enum {max_frame_msg_count = 256};

    RakNet::Packet* p;
    // GetPacketIdentifier returns this
    unsigned char packetIdentifier;

    int i = 0;
    for ( ; i < max_frame_msg_count; ++i)
    {
        p = server_->Receive();
        if (p == nullptr)
            break;

        // 开始处理消息
        packetIdentifier = p->data[0];
        // Check if this is a network message packet
        switch (packetIdentifier)
        {
        case ID_DISCONNECTION_NOTIFICATION:
            // Connection lost normally
            printf("ID_DISCONNECTION_NOTIFICATION from %s\n", p->systemAddress.ToString(true));
            this->RemoveConnection(p->guid);
            break;

        case ID_NEW_INCOMING_CONNECTION:
            // Somebody connected.  We have their IP now
            printf("ID_NEW_INCOMING_CONNECTION from %s with GUID %s\n", p->systemAddress.ToString(true), p->guid.ToString());
            AddNewConnection(p->guid);
            break;

        case ID_INCOMPATIBLE_PROTOCOL_VERSION:
            printf("ID_INCOMPATIBLE_PROTOCOL_VERSION\n");
            break;

        case ID_CONNECTED_PING:
        case ID_UNCONNECTED_PING:
            printf("Ping from %s\n", p->systemAddress.ToString(true));
            break;

        case ID_CONNECTION_LOST:
            // Couldn't deliver a reliable packet - i.e. the other system was abnormally
            // terminated
            printf("ID_CONNECTION_LOST from %s\n", p->systemAddress.ToString(true));;
            this->RemoveConnection(p->guid);
            break;
        case ID_USER_PACKET_ENUM:
            {
                //server_->Send(reinterpret_cast<char*>(p->data), p->length, HIGH_PRIORITY, RELIABLE_ORDERED, 0, p->guid, false);
                auto it = guid_conn_.find(p->guid);
                if (it != guid_conn_.end())
                {
                    auto& conn = it->second;
                    int ret = conn->OnRead(reinterpret_cast<char*>(&p->data[1]), p->length - 1);
                    if (ret < 0)
                        CloseConnection(conn->session_id());
                }
            }
            break;
        default:
            // The server knows the static data of all clients, so we can prefix the message
            // With the name data
            //printf("%s\n", p->data);

            // Relay the message.  We prefix the name for other clients.  This demonstrates
            // That messages can be changed on the server before being broadcast
            // Sending is the same as before
            LOG_ERR("recv unhandle raknet packet %d", p->data[0]);
            break;
        }
        //
        server_->DeallocatePacket(p);
    }
    return i;
}

void UdpServer::InitPollTimer()
{
    poll_timer_.expires_from_now(boost::posix_time::millisec(1));
    poll_timer_.async_wait(boost::bind(&UdpServer::PollTimerHandler, this, boost::asio::placeholders::error));
}

void UdpServer::PollTimerHandler( const boost::system::error_code& ec )
{
    if (!ec)
    {
        poll_timer_.expires_from_now(boost::posix_time::millisec(1));
        poll_timer_.async_wait(boost::bind(&UdpServer::PollTimerHandler, this, boost::asio::placeholders::error));
        Poll();
    }
    else
    {
        if (ec.value() != boost::asio::error::operation_aborted)
        {
            LOG_ERR("timer ec %d: %s", ec.value(), ec.message().c_str());
        }
    }
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
            RemoveConnection(conn->raknet_guid());
        }
    }
}

bool UdpServer::GetConnStatistics( const RakNet::RakNetGUID& conn_guid, RakNet::RakNetStatistics* stat, int* ping /*= nullptr*/ ) const
{
    auto sys_addr = server_->GetSystemAddressFromGuid(conn_guid);
    if (sys_addr == RakNet::UNASSIGNED_SYSTEM_ADDRESS)
        return false;

    server_->GetStatistics(sys_addr, stat);
    if (ping != nullptr)
    {
        *ping = server_->GetAveragePing(sys_addr);
    }
    return true;
}

void UdpServer::ExecuteUConnTimerFunc( UConnection* conn ) const
{
    uconn_timer_func_(conn);
}


} // namespace net
} // namespace z

