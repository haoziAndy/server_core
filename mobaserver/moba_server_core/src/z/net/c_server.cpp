#include "stdafx.h"
#include "c_server.h"
#include "c_connection.h"
#include "msg_header.h"
#include "msg_handler.h"
#include "z_server.h"
#include "session_mgr.h"

namespace z {
namespace net {

bool CCServer::Init( const std::string& address, const std::string& port, ICMsgHandler* handler)
{
    if (handler == nullptr)
    {
        LOG_ERR("icmsg handler == nullptr");
        return false;
    }
    if (IServer::Init(address, port) < 0)
        return false;

    request_handler_ = handler;
    // 登录超时默认1秒
    login_time_out_sec_ = 1;
    // 默认超时设置为300秒
    keepalive_time_out_sec_ = 300;
    ZSERVER.RegisterTimerFunctor(boost::bind(&SessionMgr::RemoveExpiredSessionData, &SESSION_MGR));

    return true;
}

void CCServer::SendToSession( int session_id, const std::string & user_id, SMsgHeader* msg )
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
    memcpy(cmsg+1, msg+1, msg->length);

    SendToSession(session_id, user_id, cmsg);
}

void CCServer::SendToSession( int session_id, const std::string & user_id, CMsgHeader* msg )
{
    auto it = connection_mgr_.find(session_id);
    if (it == connection_mgr_.end())
    {
        LOG_DEBUG("Stop SendMsg[%d] to session[%d] user %s: not found client session",
            msg->msg_id, session_id, user_id);
        ZPOOL_FREE(msg);
        return;
    }
    CConnection* conn = static_cast<CConnection*>(it->second);
    if (!conn)
    {
        LOG_ERR("NULL connection by player %s", user_id.c_str());
        ZPOOL_FREE(msg);
        return;
    }
    if (!user_id.empty() && conn->user_id() != user_id)
    {
        LOG_ERR("Stop SendMsg[%d] to session[%d]: user_id mismatched. need %" PRId64 ", get %" PRId64,
            msg->msg_id, session_id, user_id, conn->user_id());
        ZPOOL_FREE(msg);
        return;
    }

    auto length = msg->length + sizeof(*msg);

	msg->length += sizeof(msg->msg_id);//由于客户端消息id也算在包长里面

    CMsgHeaderHton(msg);
    conn->AsyncSend(reinterpret_cast<const char*>(msg), length);
}

} // namespace net
} // namespace z

