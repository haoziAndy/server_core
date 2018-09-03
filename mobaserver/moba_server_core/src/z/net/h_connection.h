#ifndef Z_NET_H_CONNECTION_H
#define Z_NET_H_CONNECTION_H

#include "i_connection.h"
#include "z/net/http/http_request.h"
#include "z/net/http/http_reply.h"
#include "z/net/http/http_request_parser.h"

namespace z {
namespace net {


// http 连接 
class HConnection : public IConnection
{    
public:
    HConnection(IServer* server, int conn_index, int32 timeout_sec);
    virtual ~HConnection();
    
    virtual void Start();

    virtual int32 OnRead(char* data, int32 length);

    virtual void OnWrite(int32 length);

    http::Reply& Reply() {return reply_;}

    void SendReply();
private:
    /// The incoming request.
    http::Request request_;

    /// The parser for the incoming request.
    http::RequestParser request_parser_;

    /// The reply to be sent back to the client.
    http::Reply reply_;

    /// timeout sec
    int32 timeout_sec_;
};


}; // namespace net
}; // namespace z

#endif  // Z_NET_H_CONNECTION_H
