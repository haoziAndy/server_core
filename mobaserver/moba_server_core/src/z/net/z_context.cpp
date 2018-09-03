#include "stdafx.h"
#include "z_context.h"

namespace z
{

namespace net
{





int ZContextS::InitZContext()
{
    if (context_ == NULL)
        context_ = zmq_ctx_new();
    
    if (!context_)
        return -1;

    if (zmq_ctx_set(context_, ZMQ_IO_THREADS, 1) != 0)
    {
        LOG_ERR("zmq_ctx_set(context_, ZMQ_IO_THREADS, 1) failed.");
    }

    return 0; 
}

int ZContextS::DestroyZContext()
{
    if (context_)
    {
        int ret = zmq_ctx_destroy(context_);
        if (ret)
            return -1;
        context_ = NULL;
    }
    return 0;
}


}; // namespace net
}; // namespace z