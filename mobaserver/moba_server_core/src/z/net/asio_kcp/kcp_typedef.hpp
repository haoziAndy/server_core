#pragma once

#define ASIO_KCP_CONNECTION_TIMEOUT_TIME 10 * 1000 // default is 10 seconds.
#define  ASIO_KCP_FLAGS_SNDWND  256 //发送窗口 256
#define  ASIO_KCP_FLAGS_RCVWND  256 //接收窗口 256

#include <stdint.h>
#include <string>
#include <memory>
#include "ikcp.h"

struct IKCPCB;
typedef struct IKCPCB ikcpcb;
typedef uint32_t kcp_conv_t;

// indicate a converse between a client and connection_obj between server.

namespace kcp_svr
{

    enum eEventType
    {
        eConnect,
        eDisconnect,
        eRcvMsg,
        eLagNotify,

        eCountOfEventType
    };

    const char* eventTypeStr(eEventType eventType);

    typedef void(event_callback_t)(kcp_conv_t /*conv*/, eEventType /*event_type*/, std::shared_ptr<std::string> /*msg*/);
}
