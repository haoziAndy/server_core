#ifndef Z_NET_UDP_SERVER_H
#define Z_NET_UDP_SERVER_H

#include <raknet/RakNetTypes.h>

namespace RakNet {
    struct RakNetStatistics;
}

namespace z {
namespace net {

class IUMsgHandler;
struct SMsgHeader;
struct CMsgHeader;
class UConnection;

class UdpServer
{
public:
    ~UdpServer()
    {}

    bool Init(const int32 port, const int32 max_incomming_conn, const std::string& password, IUMsgHandler* handler);

    void Destroy();

    void Run();

    void Stop();

    z::net::IUMsgHandler* request_handler() const {return request_handler_;}

    int32 AddNewConnection(const struct RakNet::RakNetGUID& conn_guid);
    int32 RemoveConnection(const struct RakNet::RakNetGUID& conn_guid);

    void CloseConnection(int32 session_id);

    void SendToSession(int session_id, uint64 user_id, SMsgHeader* msg);
    boost::shared_ptr<UConnection> GetConnection(int32 session_id) const;

    void SetUConnTimerFunc(boost::function<void (UConnection*)>&& func)
    {
        uconn_timer_func_ = func;
    }
    void ExecuteUConnTimerFunc(UConnection* conn) const;

    // 连接状态数据
    bool GetConnStatistics(const RakNet::RakNetGUID& conn_guid, RakNet::RakNetStatistics* stat, int* ping = nullptr) const;
private:
    void SendToSession(int session_id, uint64 user_id, CMsgHeader* msg);

    void InitPollTimer();
    void PollTimerHandler(const boost::system::error_code& ec);
    int32 Poll();

    struct HashGuidPair
    {        
        size_t operator()(const struct RakNet::RakNetGUID& id ) const
        {
            return std::hash<uint64>()(id.g);
        }
    };
    struct EqualGuidPair
    {
        bool operator () (const struct RakNet::RakNetGUID& id1,
            const struct RakNet::RakNetGUID& id2) const
        {
            return id1.g == id2.g;
        }
    };

private:
    RakNet::RakPeerInterface* server_;
    z::net::IUMsgHandler* request_handler_;

    std::unordered_map<int32, boost::shared_ptr<UConnection>> session_conn_;
    std::unordered_map<struct RakNet::RakNetGUID, boost::shared_ptr<UConnection>, HashGuidPair, EqualGuidPair> guid_conn_;
    
    /// The signal_set is used to register for process termination notifications.
    boost::asio::signal_set signals_;    

    bool is_server_shutdown_;

    boost::asio::deadline_timer poll_timer_;

    boost::function<void (UConnection*)> uconn_timer_func_;
private:
    UdpServer();
        
    DECLARE_SINGLETON(UdpServer);
};


} // namespace net
} // namespace z

#define UDPSERVER Singleton<z::net::UdpServer>::instance()


#endif // Z_NET_UDP_SERVER_H

