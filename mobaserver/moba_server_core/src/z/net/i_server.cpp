#include "stdafx.h"

#include "msg_handler.h"
#include "i_connection.h"
#include "i_server.h"
#include "z_server.h"

namespace z {
namespace net {

IServer::IServer()
    : master_io_service_(ZSERVER.time_engine())
    , work_(master_io_service_)
    , acceptor_(master_io_service_)
    , signals_(master_io_service_)
    , is_server_shutdown_(false)
    , new_connection_(nullptr)
{}

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

    if (new_connection_ != nullptr)
    {
        ZPOOL_DELETE(new_connection_);
        new_connection_ = nullptr;
    }

    for (auto it = connection_mgr_.begin(); it != connection_mgr_.end(); ++it)
    {
        if (it->second != nullptr)
        {
            ZPOOL_DELETE(it->second);
        }
    }
    connection_mgr_.clear();
}

void IServer::StartAccept()
{
    if (is_server_shutdown_)
        return;
    static int s_conn_index = 0;
    if (!new_connection_)
    {
//         new_connection_ = ZPOOL_NEW(CConnection, std::ref(*workers_[next_work_index_]), conn_index++, 
//             c_in_msg_handler_, c_out_msg_handler_);
        auto conn_index = ((++s_conn_index) << 1) | 0x1;
        new_connection_ = CreateConnection(conn_index++);
        
        // 42亿个id, 应该不会被占满, 做个保护, 最大循环100
        for (int i=0; connection_mgr_.find(conn_index) != connection_mgr_.end() && i<100; ++i)
            ++ conn_index;
    }
    acceptor_.async_accept(new_connection_->socket(), 
        boost::bind(&IServer::HandleAccept, this, boost::asio::placeholders::error));
}

void IServer::HandleAccept( const boost::system::error_code& ec )
{
    if (is_server_shutdown_)
        return;

    if (!ec)
    {
        try
        {
            int32 session_id = new_connection_->session_id();
            LOG_DEBUG("Client[%d] addr %s[%d] connected.", session_id,
                new_connection_->socket().remote_endpoint().address().to_string().c_str(),
                new_connection_->socket().remote_endpoint().port());
            auto ret = connection_mgr_.insert(std::make_pair(session_id, new_connection_));
            if (ret.second)
            {
                new_connection_->Start();
                new_connection_ = nullptr;                
            }
            else
            {
                LOG_ERR("%d session already used on client connected", session_id);
                ZPOOL_DELETE(new_connection_);
                new_connection_ = nullptr;
            }
            
        } 
        catch(boost::system::system_error& remote_ec)
        {
            LOG_ERR("%d: %s", remote_ec.code().value(), remote_ec.what());
            boost::system::error_code t_ec;
            new_connection_->socket().close(t_ec);
        }
    }
    
    StartAccept();
}

void IServer::CloseConnection( int32 session_id )
{
    auto it = connection_mgr_.find(session_id);
    if (it != connection_mgr_.end())
    {
        LOG_DEBUG("session[%d] end.", session_id);
        auto conn = it->second;
        conn->Close();
        connection_mgr_.erase(it);
    }
    else
    {
        LOG_ERR("not found session %d on CloseConnection", session_id);
    }
}

IConnection* IServer::GetConnection( int32 session_id ) const
{
    auto it = connection_mgr_.find(session_id);
    if (it != connection_mgr_.end())
        return it->second;
    else
        return nullptr;
}


} //namespace net
} //namespace z
