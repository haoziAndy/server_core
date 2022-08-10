#ifndef NET_MSG_HEADER_H
#define NET_MSG_HEADER_H

// 短连接模式
#define NON_PERSISTANCE_MODE 0 //修改了很多 此版本永远不要开这个模式
#define UIDLen 22


#define INNER_MSG_START_ID 50000

/*struct SMsgHeader
{
SMsgHeader()
: length(0), msg_id(0), src_server_id(0), dst_server_id(0)
, send_tick(0), recv_tick(0), seq(0), session_id(0), player_id(0)
{}
uint32    length;
uint16    msg_id;
uint16    src_server_id;        // 消息来源 服务器id
uint16    dst_server_id;        // 消息目的 服务器id
uint16    send_tick;            // 消息发送client tick,
uint16    recv_tick;            // 消息服务器收到 tick,
uint64    seq;                  // 唯一标记该消息, 每查询一次递增
uint32    session_id;           // 用于gate快速查找session
uint64    player_id;            // 玩家未登录时，player_id为0
};*/

namespace z
{
namespace net
{

#pragma pack(push, 1)


struct SMsgHeader
{
	SMsgHeader()
		: msg_id(0), length(0),  src_server_id(0), dst_server_id(0), msg_stream_id(0), session_id(0)
	{
		async_seq[0] = '\0';
		player_id[0] = '\0';
	}
	uint32    msg_id;
	uint32    length;
	int32    src_server_id;        // 消息来源 服务器id 
	int32    dst_server_id;        // 消息目的
	uint32   msg_stream_id;			//用于发给前端的消息流ID
	uint32    session_id;        // 为gate 登录sessionid
	char    async_seq[UIDLen];   //结尾并不是 '\0'
	char    player_id[UIDLen];   // 玩家未登录时，player_id为0,结尾并不是 '\0'
};


struct CMsgHeader
{
#if NON_PERSISTANCE_MODE
    CMsgHeader()
        : length(0), msg_id(0), openid(0), checksum(0)
    {
        memset(key, 0, sizeof(key));
    }
#else
    CMsgHeader()
        : length(0), msg_id(0), msg_stream_id(0)/*, send_tick(0), checksum(0)*/
    {}
#endif
	uint16 length;
	uint16 msg_id;
	uint32 msg_stream_id;			//用于发给前端的消息流ID
#if NON_PERSISTANCE_MODE
    uint64 openid;      // 
    uint16 checksum;    // 防止重放攻击    
    char key[16];       // session key
#else
   // uint16 send_tick;
    //uint16 checksum;    // 防止重放攻击   
#endif

};
struct SCMsgHeader
{
    SCMsgHeader()
        : length(0), msg_id(0)
    {}

	uint16 length;
	uint16 msg_id;
};

#ifdef _WIN32
#define bswap64 _byteswap_uint64
#else
#define bswap64 __builtin_bswap64
#endif 

#ifdef BOOST_ENDIAN_LITTLE_BYTE_AVAILABLE
# if NON_PERSISTANCE_MODE
#  define CMsgHeaderNtoh(header) do {\
    header->length = boost::asio::detail::socket_ops::network_to_host_short(header->length);\
    header->msg_id = boost::asio::detail::socket_ops::network_to_host_short(header->msg_id);\
    header->openid = bswap64(header->openid);\
    header->checksum = boost::asio::detail::socket_ops::network_to_host_short(header->checksum);\
    } while (false)

#  define CMsgHeaderHton(header) do {\
    header->length = boost::asio::detail::socket_ops::host_to_network_short(header->length);\
    header->msg_id = boost::asio::detail::socket_ops::host_to_network_short(header->msg_id);\
    header->openid = bswap64(header->openid);\
    header->checksum = boost::asio::detail::socket_ops::host_to_network_short(header->checksum);\
} while (false)
# else
#  define CMsgHeaderNtoh(header) do {\
    header->length = boost::asio::detail::socket_ops::network_to_host_short(header->length);\
    header->msg_id = boost::asio::detail::socket_ops::network_to_host_short(header->msg_id);\
	header->msg_stream_id = boost::asio::detail::socket_ops::network_to_host_long(header->msg_stream_id);\
} while (false)

/*header->send_tick = boost::asio::detail::socket_ops::network_to_host_short(header->send_tick);
header->checksum = boost::asio::detail::socket_ops::network_to_host_short(header->checksum);*/

#  define CMsgHeaderHton(header) do {\
    header->length = boost::asio::detail::socket_ops::host_to_network_short(header->length);\
    header->msg_id = boost::asio::detail::socket_ops::host_to_network_short(header->msg_id);\
	header->msg_stream_id = boost::asio::detail::socket_ops::host_to_network_long(header->msg_stream_id);\
} while (false)

/* header->send_tick = boost::asio::detail::socket_ops::host_to_network_short(header->send_tick);
header->checksum = boost::asio::detail::socket_ops::host_to_network_short(header->checksum);*/


# endif // NON_PERSISTANCE_MODE


#else
#  define CMsgHeaderNtoh(header)
#  define CMsgHeaderHton(header)

#  define SCMsgHeaderNtoh(header)
#  define SCMsgHeaderHton(header)
#endif // BOOST_LITTLE_ENDIAN

#pragma pack(pop)


} // namespace net
} // namespace z

#endif //NET_MSG_HEADER_H
