#include "stdafx.h"
#include "z_sender.h"
#include "msg_header.h"
#include "z_context.h"
#include "z_server.h"
#include "z/common/public_function.h"


namespace z {
namespace net {

ZSender::ZSender()
    : socket_(NULL)
    , sendto_server_id_(0)
{
}

int ZSender::Init( const int sendto_server_id, int zsocket_type, const std::string& conn_str, const std::string& sender_name )
{
    ZContext* context = ZCONTEXT.context();
    if (!context)
    {
        LOG_ERR("ZCONTEXT is null.");
        return -1;
    }
    socket_ = zmq_socket(context, zsocket_type);
    if (!socket_)
    {
        LOG_ERR("zmq_socket() failed. %s", zmq_strerror(zmq_errno()));
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
    ret = zmq_connect(socket_, conn_str.c_str());
    if (ret != 0)
    {
        LOG_ERR("zmq_connect() failed. %s", zmq_strerror(zmq_errno()));
        return -1;
    }

    sendto_server_id_ = sendto_server_id;
    sender_name_ = sender_name;
    return 0;
}

int ZSender::Close(bool force)
{
    if (socket_)
    {
        if (force)
        {
            int wait_ms = 1;
            int ret = zmq_setsockopt(socket_, ZMQ_LINGER, &wait_ms, sizeof(wait_ms));
            if (ret)
            {
                LOG_ERR("zmq_setsockopt() failed. %s", zmq_strerror(zmq_errno()));
                return -1;
            }
        }
        int ret = zmq_close(socket_);
        if (ret)
        {
            LOG_ERR("zmq_close() failed. %s", zmq_strerror(zmq_errno()));
            return -1;
        }
        socket_ = NULL;
    }
    return 0;
}

int ZSender::Send( SMsgHeader* header, const google::protobuf::Message* protobuf_msg )
{
#if(defined WIN32 or defined DEBUG)
	ZSERVER.PrintMsg(header, protobuf_msg,true);
#endif

    // set sender id
    header->src_server_id = ZSERVER.server_id();
    header->dst_server_id = sendto_server_id_;

    int byte_size = protobuf_msg->ByteSize();
    header->length = byte_size;

    int ret;    
    zmq_msg_t msg;    
    ret = zmq_msg_init_size(&msg, sizeof(*header) + header->length);
    if (ret != 0)
    {
        LOG_ERR("zmq_msg_init_size failed. %d: %s", zmq_errno(), zmq_strerror(zmq_errno()));
        return -1;
    }
    char* data_buf = reinterpret_cast<char*>(zmq_msg_data(&msg));
    memcpy(data_buf, header, sizeof(*header));
    if (!protobuf_msg->SerializeToArray(data_buf + sizeof(*header), header->length))
    {
        zmq_msg_close(&msg);
        return -1;
    }
    ret = zmq_msg_send(&msg, socket_, ZMQ_DONTWAIT);

    zmq_msg_close(&msg);
    
    return ret;
}

int ZSender::Send( SMsgHeader* s_msg )
{
#if(defined WIN32 or defined DEBUG)
	ZSERVER.PrintMsg(s_msg,true);
#endif

    s_msg->src_server_id = ZSERVER.server_id();
    s_msg->dst_server_id = sendto_server_id_;


    int ret;    
    zmq_msg_t msg;    
    ret = zmq_msg_init_size(&msg, sizeof(*s_msg) + s_msg->length);
    if (ret != 0)
        return -1;

    memcpy(zmq_msg_data(&msg), s_msg, sizeof(*s_msg) + s_msg->length);

    ret = zmq_msg_send(&msg, socket_, ZMQ_DONTWAIT);

    zmq_msg_close(&msg);

    return ret;
}



} //namespace net
} //namespace z
