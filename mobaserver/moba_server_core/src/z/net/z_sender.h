#ifndef Z_NET_Z_SENDER_H
#define Z_NET_Z_SENDER_H

#include "z_context.h"

namespace z {
namespace net {

struct SMsgHeader;


class ZSender
{
public:
    explicit ZSender();

    //lint -sem(z::net::Sender::Destroy,cleanup)
    ~ZSender()
    {
        //lint -e534
        Close();
    }

    int Init(const int sendto_server_id, int zsocket_type, const std::string& conn_str, const std::string& sender_name);

    int Close();

    /// 需要序列化再发送
    int Send(SMsgHeader* header, const google::protobuf::Message* msg);
    /// 转发, 消息已经序列化
    int Send(SMsgHeader* msg);

    ZSocket* socket() const {return socket_;}
    const std::string& name() const {return sender_name_;}

    void set_rev_msg_time(int64 rev_msg_time) { rev_msg_time_ = rev_msg_time;}
	int64 last_rev_msg_time() const {return rev_msg_time_;}

	int32 get_sendto_server_id() const { return sendto_server_id_; }
private:
    ZSocket* socket_;    
    int sendto_server_id_; 
    std::string sender_name_;

	int64 rev_msg_time_;

	
};

}; //namespace net
}; //namespace z



#endif //Z_NET_Z_SENDER_H