#include "stdafx.h"
#include "h_server.h"

namespace z {
namespace net {


bool HServer::Init( const std::string& address, const std::string& port, z::net::IHttpRequestHandler* handler, int32 timeout_sec )
{
    if (handler == nullptr)
    {
        LOG_ERR("HServer requst handler == nullptr");
        return false;
    }
    if (IServer::Init(address, port) < 0)
        return false;
    
    request_handler_ = handler;
    timeout_sec_ = timeout_sec;
    return true;
}

void HServer::SendToSession( int32 session_id, const std::string& data )
{
    auto it = connection_mgr_.find(session_id);
    if (it == connection_mgr_.end())
        return;
    HConnection* conn = static_cast<HConnection*>(it->second);
    if (!conn)
    {
        LOG_DEBUG("not found client session %d", session_id);
        return;
    }
    conn->Reply().SetContent(data);
    conn->SendReply();
}


} // namespace net
} // namespace z
