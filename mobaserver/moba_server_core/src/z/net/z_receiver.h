#ifndef Z_NET_Z_RECEIVER_H
#define Z_NET_Z_RECEIVER_H

#include "z_context.h"

namespace z
{
namespace net
{

class ISMsgHandler;
struct SMsgHeader;

class ZReceiver
{
public:
    explicit ZReceiver()
        : socket_(NULL)
        , server_id_(0)
        , handler_(NULL)
    {
        memset(&zmsg_, 0, sizeof(zmsg_));
    }

    ~ZReceiver()
    {
        Destroy();
    }

    int Init(const int server_id, int zsocket_type, const char* bind_str);

    int Destroy();

    void SetSMsgHandler(ISMsgHandler* handler)
    {
        handler_ = handler;
    }

    int ReceiveMessage();

    int HandlerReceived();

    ZSocket* socket() 
    {
        return socket_;
    }
    int server_id() const {return server_id_;}

    const zmq_msg_t& received_zmsg() const {return zmsg_;}
private:
    ZSocket* socket_;    
    int server_id_;
    ISMsgHandler* handler_;
    zmq_msg_t zmsg_;

};



}; //namespace net
}; //namespace z



#endif //Z_NET_Z_RECEIVER_H

