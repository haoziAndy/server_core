#ifndef Z_NET_UDP_SERVER_H
#define Z_NET_UDP_SERVER_H

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

    bool Init(const std::string& addr, const std::string& port, const int32 max_incomming_conn, IUMsgHandler* handler);

    void Destroy();

    void Run();

    void Stop();

    z::net::IUMsgHandler* request_handler() const {return request_handler_;}

    int32 AddNewConnection();
    int32 RemoveConnection();

    void CloseConnection(int32 session_id);

    void SendToSession(int session_id, uint64 user_id, SMsgHeader* msg);
    boost::shared_ptr<UConnection> GetConnection(int32 session_id) const;

    void SetUConnTimerFunc(boost::function<void (UConnection*)>&& func)
    {
        uconn_timer_func_ = func;
    }
    void ExecuteUConnTimerFunc(UConnection* conn) const;

    // 连接状态数据
   // bool GetConnStatistics(const RakNet::RakNetGUID& conn_guid, RakNet::RakNetStatistics* stat, int* ping = nullptr) const;
private:
    void SendToSession(int session_id, uint64 user_id, CMsgHeader* msg);

	void HookUdpAsyncReceive(void);
	void HandleUdpReceiveFrom(const boost::system::error_code& error, size_t bytes_recvd);
    void PollTimerHandler(const boost::system::error_code& ec);

private:
	/// The listen socket.
	boost::asio::ip::udp::socket udp_socket_;
	boost::asio::ip::udp::endpoint udp_remote_endpoint_;

    z::net::IUMsgHandler* request_handler_;

    std::unordered_map<int32, boost::shared_ptr<UConnection>> session_conn_;

    /// The signal_set is used to register for process termination notifications.
    boost::asio::signal_set signals_;    

    bool is_server_shutdown_;

	enum { udp_packet_max_length = 1080 }; // (576-8-20 - 8) * 2
	char udp_data_[1024 * 32];

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

