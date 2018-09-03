#ifndef PENGDING_MSG_QUEUE_H_
#define PENGDING_MSG_QUEUE_H_

namespace z { namespace net { 
    struct SMsgHeader;
    class ISMsgHandler;
};};

namespace z {
namespace utils{


class PendingMsgQueue
{
public:
    ~PendingMsgQueue();

    void Pending(const z::net::SMsgHeader* msg, int64 recv_tick);
    
    void Dispatch(z::net::ISMsgHandler* handler);

private:
    std::list<std::pair<z::net::SMsgHeader*, int64> > pending_msg_queue_;

    DECLARE_SINGLETON_CONSTRUCTER(PendingMsgQueue);
};

}; // namespace utils
}; // namespace z

#define PENDING_QUEUE Singleton<z::utils::PendingMsgQueue>::instance()

#endif // PENGDING_MSG_QUEUE_H_
