#ifndef Z_NET_I_SERVER_H
#define Z_NET_I_SERVER_H


namespace z {
namespace net {

class IConnection;
class ZReceiver;
class ZSender;
class ISMsgHandler;

class IServer
{
    // typedef std::map<int, ZReceiver*> Receivers;
    // typedef std::vector<ZSender*> Senders;
public:
    IServer();
    virtual ~IServer()
    {
        Destroy();
    }
protected:
    int Init(const std::string& address, const std::string& port);
public:
    void Destroy();

    //void Run();

    void Stop();

    void StartAccept();

    void HandleAccept(const boost::system::error_code& ec);

    virtual IConnection* CreateConnection(int32 session_id) = 0;

    void CloseConnection(int32 session_id);

    IConnection* GetConnection(int32 session_id) const;

	const std::unordered_map<int, IConnection*> GetALlConnection() const {
		return connection_mgr_;
	};

    boost::asio::io_service& io_service() { return master_io_service_; }

protected:
    boost::asio::io_service& master_io_service_;
    boost::asio::io_service::work work_;

    /// Acceptor used to listen for incoming connections.
    boost::asio::ip::tcp::acceptor acceptor_;

    /// The signal_set is used to register for process termination notifications.
    boost::asio::signal_set signals_;    

    bool is_server_shutdown_;

    IConnection* new_connection_;

    std::unordered_map<int, IConnection*> connection_mgr_;

    DISALLOW_COPY_AND_ASSIGN(IServer);
};

} // namespace net
} // namespace z


#endif //Z_NET_I_SERVER_H

