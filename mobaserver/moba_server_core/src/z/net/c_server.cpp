#include "stdafx.h"
#include "c_server.h"
#include "c_connection.h"
#include "msg_header.h"

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
    // ��¼��ʱĬ��1��
    login_time_out_sec_ = 1;
    // Ĭ�ϳ�ʱ����Ϊ300��
    keepalive_time_out_sec_ = 300;

    return true;
}

} // namespace net
} // namespace z

