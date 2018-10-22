#include "stdafx.h"
#include "z_receiver.h"
#include "msg_handler.h"
#include "msg_header.h"
#include "z_server.h"

namespace z {
namespace net {

int ZReceiver::Init( const int server_id, int zsocket_type, const char* addr )
{
    ZContext* context = ZCONTEXT.context();
    if (!context)
    {
        LOG_ERR("zcontext is null");
        return -1;
    }
    socket_ = zmq_socket(context, zsocket_type);
    if (!socket_)
    {
        LOG_ERR("zmq_socket failed, %d: %s", zmq_errno(), zmq_strerror(zmq_errno()));
        return -1;
    }
    int ret;
    int hwm = 0;
    ret = zmq_setsockopt(socket_, ZMQ_SNDHWM, &hwm, sizeof(hwm));
    if (ret != 0)
    {
        LOG_ERR("zmq_setsockopt failed, %d: %s", zmq_errno(), zmq_strerror(zmq_errno()));
        return -1;
    }
    ret = zmq_setsockopt(socket_, ZMQ_RCVHWM, &hwm, sizeof(hwm));
    if (ret != 0)
    {
        LOG_ERR("zmq_setsockopt failed, %d: %s", zmq_errno(), zmq_strerror(zmq_errno()));
        return -1;
    }
    ret = zmq_bind(socket_, addr);
    if (ret != 0)
    {
        LOG_ERR("zmq bind failed, %d: %s", zmq_errno(), zmq_strerror(zmq_errno()));
        return -1;
    }

    server_id_ = server_id;

    return 0;
}

int ZReceiver::Destroy()
{
    if (socket_)
    {
        int ret = zmq_close(socket_);
        if (ret != 0)
        {
            LOG_ERR("zmq_close failed, %d: %s", zmq_errno(), zmq_strerror(zmq_errno()));
            return -1;
        }
        socket_ = NULL;
    }
    
    return 0;
}

int ZReceiver::ReceiveMessage()
{
    zmq_msg_t* msg = &zmsg_;
    int ret = zmq_msg_init(msg);
    if (ret != 0)
    {
        LOG_ERR("zmq_msg_init failed, %d: %s", zmq_errno(), zmq_strerror(zmq_errno()));
        return -1;
    }
    int size = zmq_msg_recv(msg, socket_, ZMQ_DONTWAIT);
    if (size < 0)
    {
        int err = zmq_errno();
        zmq_msg_close(msg);
        if (err == EAGAIN)
        {
            return 0;
        }
        else
        {
            LOG_ERR("zmq_msg_recv failed, %d: %s", err, zmq_strerror(err));
            return -1;
        }
    }
    return size;
}

int ZReceiver::HandlerReceived()
{
    assert(handler_);
	SMsgHeader* msg = reinterpret_cast<SMsgHeader*>(zmq_msg_data(&zmsg_));
	if (!msg)
        return -1;

#if(defined WIN32 or defined DEBUG)
	ZSERVER.PrintMsg(msg,false);
#endif

    int ret = handler_->OnMessage(msg, TIME_ENGINE.time());
    
    zmq_msg_close(&zmsg_);
    
    return ret;
}







} //namespace net
} //namespace z
