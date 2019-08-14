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

    bool Init(const std::string& addr, const std::string& port, IUMsgHandler* handler);

    void Destroy();

    void Run();

    void Stop();

    z::net::IUMsgHandler* request_handler() const {return request_handler_;}

    int32 AddNewConnection(const kcp_conv_t _kcp_conv_t);

    void CloseConnection(int32 session_id);

    void SendToSession(int session_id, const std::string & user_id, SMsgHeader* msg);
    boost::shared_ptr<UConnection> GetConnection(int32 session_id);

	int32 login_time_out_sec() const { return login_time_out_sec_; }
	void set_login_time_out_sec(int32 timeout_sec) { login_time_out_sec_ = timeout_sec; }

	int32 keepalive_time_out_sec() const { return keepalive_time_out_sec_; }
	void set_keepalive_time_out_sec(int32 time_out_sec) { keepalive_time_out_sec_ = time_out_sec; }

private:
    void SendToSession(int session_id, const std::string & user_id, CMsgHeader* msg);
    void PollTimerHandler(const boost::system::error_code& ec);
	int32 RemoveConnection(const int32 session_id);

private:

    z::net::IUMsgHandler* request_handler_;

    std::unordered_map<int32, boost::shared_ptr<UConnection>> session_conn_;

    /// The signal_set is used to register for process termination notifications.
    boost::asio::signal_set signals_;    

	kcp_svr::server kcp_server_;

    bool is_server_shutdown_;

	int32 login_time_out_sec_;

	int32 keepalive_time_out_sec_;

private:
	UdpServer();
        
    DECLARE_SINGLETON(UdpServer);
};


} // namespace net
} // namespace z

#define UDPSERVER Singleton<z::net::UdpServer>::instance()


#endif // Z_NET_UDP_SERVER_H

