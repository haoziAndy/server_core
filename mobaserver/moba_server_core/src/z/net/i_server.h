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

    void Stop();

    virtual void StartAccept();

    int GenNewConnectionIndex(){
        static int s_conn_index = 0;
        auto conn_index = ++s_conn_index;
        return conn_index;
    }

    virtual std::shared_ptr<IConnection> CreateConnection(boost::asio::ip::tcp::socket&& sock, int32 session_id) = 0;

    virtual void CloseConnection(int32 session_id);

    virtual std::shared_ptr<IConnection> GetConnection(int32 session_id) const;

	const std::unordered_map<int, std::shared_ptr<IConnection>>& GetALlConnection() {
		return connection_mgr_;
	};

    boost::asio::io_service& io_service() { return master_io_service_; }

    void SendToSession(int session_id, const std::string & user_id, SMsgHeader* msg);

protected:
    virtual void _SendToSession(int session_id, const std::string & user_id, CMsgHeader* msg);

    boost::asio::io_service& master_io_service_;

    /// Acceptor used to listen for incoming connections.
    boost::asio::ip::tcp::acceptor acceptor_;

    /// The signal_set is used to register for process termination notifications.
    boost::asio::signal_set signals_;    

    bool is_server_shutdown_;

    static std::unordered_map<int, std::shared_ptr<IConnection>> connection_mgr_;

    DISALLOW_COPY_AND_ASSIGN(IServer);
};

} // namespace net
} // namespace z


#endif //Z_NET_I_SERVER_H

