#include "stdafx.h"
#include "pending_msg_queue.h"
#include <z/net/msg_header.h>
#include <z/net/msg_handler.h>
#include "memory_pool.h"
#include "logger.h"

namespace z {
namespace utils{


PendingMsgQueue::~PendingMsgQueue()
{
    for (auto it=pending_msg_queue_.begin(); it!=pending_msg_queue_.end(); ++it)
    {
        auto& msg_pair = *it;
        ZPOOL_FREE(msg_pair.first);
    }
    pending_msg_queue_.clear();
}

void PendingMsgQueue::Pending( const z::net::SMsgHeader* msg, int64 recv_tick )
{
    int32 length = sizeof(*msg) + msg->length;
    void* buff = ZPOOL_MALLOC(length);
    memcpy(buff, msg, length);

    pending_msg_queue_.push_back(std::make_pair(
        reinterpret_cast<z::net::SMsgHeader*>(buff),
        recv_tick));
}

void PendingMsgQueue::Dispatch( z::net::ISMsgHandler* handler )
{
    for (auto it=pending_msg_queue_.begin(); it!=pending_msg_queue_.end(); ++it)
    {
        auto& msg_pair = *it;
        handler->OnMessage(msg_pair.first, msg_pair.second);
        ZPOOL_FREE(msg_pair.first);
    }
    pending_msg_queue_.clear();
}



}; // namespace utils
}; // namespace z
