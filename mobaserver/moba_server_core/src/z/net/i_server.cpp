#include "stdafx.h"

#include "msg_handler.h"
#include "i_connection.h"
#include "i_server.h"
#include "z_server.h"

namespace z {
namespace net {

IServer::IServer()
    : master_io_service_(ZSERVER.time_engine())
    , acceptor_(master_io_service_)
    , signals_(master_io_service_)
    , is_server_shutdown_(false)
{}

std::unordered_map<int, std::shared_ptr<IConnection>> IServer::connection_mgr_;

int IServer::Init(const std::string& address, const std::string& port)
{
    // Open the acceptor with the option to reuse the address (i.e. SO_REUSEADDR).
    boost::asio::ip::tcp::resolver resolver(master_io_service_);
    boost::asio::ip::tcp::resolver::query query(address, port);

    boost::system::error_code ec;
    auto it = resolver.resolve(query, ec);
    if (ec)
    {
        LOG_ERR("IServer::Init failed. Error[%d]: %s", ec.value(), ec.message().c_str());
        return -1;
    }

    boost::asio::ip::tcp::endpoint endpoint = *it;
    
    acceptor_.open(endpoint.protocol());
#ifdef _WIN32
    {
        int optval = 1;
        if (setsockopt(acceptor_.native_handle(), SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
                (char*)&optval, sizeof(optval)))
        {
            LOG_ERR("IServer::Init failed. setsockopt SO_EXCLUSIVEADDRUSE failed.");
            return -1;
        }
    }
#else
    acceptor_.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
#endif // _WIN32

#ifdef TCP_DEFER_ACCEPT
    {
        int on = 1;
        if (setsockopt(acceptor_.native_handle(), SOL_TCP, TCP_DEFER_ACCEPT, &on, sizeof(on)) < 0)
        {
            LOG_ERR("IServer::Init failed. setsockopt TCP_DEFER_ACCEPT failed");
            return -1;
        }
    }    
#endif // TCP_DEFER_ACCEPT

    acceptor_.bind(endpoint, ec);
    if (ec)
    {
        LOG_ERR("IServer::Init failed. bind on %s:%s failed", address.c_str(), port.c_str());
        return -1;
    }
    
    acceptor_.listen();
    StartAccept();

    signals_.add(SIGINT);
    signals_.add(SIGTERM);
#if defined(SIGQUIT)
    signals_.add(SIGQUIT);
#endif // defined(SIGQUIT)
    signals_.async_wait(boost::bind(&IServer::Stop, this));

    return 0;
}

void IServer::Destroy()
{
    //Stop();
    return;
}

/*void IServer::Run()
{
    ZSERVER.Run();
}*/

void IServer::Stop()
{
    if (is_server_shutdown_)
        return;
    is_server_shutdown_ = true;
    
    boost::system::error_code ec;
    acceptor_.cancel(ec);
    acceptor_.close(ec);

    //ZSERVER.Stop();
    connection_mgr_.clear();
}

void IServer::StartAccept()
{
    if (is_server_shutdown_)
        return;

    acceptor_.async_accept(master_io_service_, [this](const boost::system::error_code& ec, boost::asio::ip::tcp::socket new_socket) {
        if (is_server_shutdown_)
            return;

        if (!ec)
        {
            try
            {
                auto session_id = GenNewConnectionIndex();
                {
                    boost::system::error_code ec_endpoint;
                    auto endpoint = new_socket.remote_endpoint(ec_endpoint);
                    if (ec_endpoint)
                    {
                        LOG_ERR("Error getting remote endpoint");
                    }
                    else
                    {
                        LOG_DEBUG("Client[%d] addr %s[%d] connected.", session_id, endpoint.address().to_string().c_str(), endpoint.port());
                    }
                }
                auto new_conn = CreateConnection(std::move(new_socket), session_id);
                auto ret = connection_mgr_.insert(std::make_pair(session_id, new_conn));
                if (ret.second)
                {
                    new_conn->Start();
                }
                else
                {
                    LOG_ERR("%d session already used on client connected", session_id);
                }

            }
            catch (boost::system::system_error& remote_ec)
            {
                LOG_ERR("%d: %s", remote_ec.code().value(), remote_ec.what());
            }
        }

        StartAccept();
    });
}

void IServer::CloseConnection( int32 session_id )
{
    auto it = connection_mgr_.find(session_id);
    if (it != connection_mgr_.end())
    {
        LOG_DEBUG("session[%d] end.", session_id);
        it->second->Close();
        connection_mgr_.erase(it);
    }
    else
    {
        LOG_DEBUG("not found session %d on CloseConnection", session_id);
    }
}

std::shared_ptr<IConnection> IServer::GetConnection( int32 session_id ) const
{
    auto it = connection_mgr_.find(session_id);
    if (it != connection_mgr_.end())
        return it->second;
    else
        return nullptr;
}

void IServer::SendToSession( int session_id, const std::string & user_id, SMsgHeader* msg )
{
	if (msg->length > 0xffff)
	{
		auto msg_names = ZSERVER.msg_names();
		if (msg_names != nullptr)
			LOG_ERR("client msg length > 0xffff, id %d %s", msg->msg_id, (*msg_names)[msg->msg_id].c_str());
		return;
	}
    z::net::CMsgHeader* cmsg;
    int buff_length = msg->length + sizeof(*cmsg);
    char* buff = reinterpret_cast<char*> (ZPOOL_MALLOC(buff_length));
    cmsg = reinterpret_cast<z::net::CMsgHeader*>(buff);
    cmsg->length = msg->length;
    cmsg->msg_id = msg->msg_id;
	cmsg->srv_msg_stream_id = msg->srv_msg_stream_id;
	cmsg->cli_msg_stream_id = msg->cli_msg_stream_id;
    memcpy(cmsg+1, msg+1, msg->length);

    _SendToSession(session_id, user_id, cmsg);
}

void IServer::_SendToSession(int session_id, const std::string& user_id, CMsgHeader* msg)
{
    auto it = connection_mgr_.find(session_id);
    if (it == connection_mgr_.end())
    {
        LOG_DEBUG("Stop SendMsg[%d] to session[%d] user %s: not found client session",
            msg->msg_id, session_id, user_id.c_str());
        ZPOOL_FREE(msg);
        return;
    }
    auto& conn = it->second;
    if (!conn)
    {
        LOG_ERR("NULL connection by player %s", user_id.c_str());
        ZPOOL_FREE(msg);
        return;
    }
    if (!user_id.empty() && conn->user_id() != user_id)
    {
        LOG_DEBUG("Stop SendMsg[%d] to session[%d]: user_id mismatched. need %s, get %s",
            msg->msg_id, session_id, user_id.c_str(), conn->user_id().c_str());
        ZPOOL_FREE(msg);
        return;
    }

    auto length = msg->length + sizeof(*msg);

    CMsgHeaderHton(msg);
    conn->AsyncSend(reinterpret_cast<const char*>(msg), length);
}


} //namespace net
} //namespace z
