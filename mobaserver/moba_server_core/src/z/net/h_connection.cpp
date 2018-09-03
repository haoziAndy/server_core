#include "stdafx.h"
#include "h_connection.h"
#include "i_server.h"
#include "h_server.h"
#include "msg_handler.h"

namespace z {
namespace net {

HConnection::HConnection( IServer* server, int conn_index, int32 timeout_sec )
    : IConnection(server, conn_index)
    , timeout_sec_(timeout_sec)
{
    
}

HConnection::~HConnection()
{
}

void HConnection::Start()
{
    ++op_count_;
    deadline_timer_.expires_from_now(boost::posix_time::seconds(timeout_sec_));
    deadline_timer_.async_wait(
        boost::bind(&HConnection::AsyncClose, this));

    IConnection::Start();
}

int32 HConnection::OnRead( char* data, int32 length )
{
    http::RequestParser::result_type result;
    std::tie(result, std::ignore) = request_parser_.Parse(
        request_, data, data + length);

    if (result == http::RequestParser::good)
    {
        http::RequestUri uri;
        if (http::RequestParser::ParseRequestUri(request_.uri, &uri))
        {
            uri.post_data = request_.content;
            int ret = HTTP_SERVER.request_handler()->OnRequest(this, uri);
            if (ret < 0)
                return -1;
            else
                return length;
        }
        else
        {
            return -1;
        }
    }
    else if (result == http::RequestParser::bad)
    {
        reply_ = http::Reply::StockReply(http::Reply::bad_request);
        AsyncSend(reply_.to_buffers());
        return length;
    }
    else
    {
        return 0;
    }
}

void HConnection::OnWrite( int32 length )
{
    AsyncClose();
}

void HConnection::SendReply()
{
    AsyncSend(reply_.to_buffers());
}


} // namespace net
} // namespace z

