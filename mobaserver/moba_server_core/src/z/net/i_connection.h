#ifndef Z_NET_I_CONNECTION_H
#define Z_NET_I_CONNECTION_H

#include "z_context.h"
#include <atomic>
namespace z {
namespace net {


class IServer;
struct SMsgHeader;

// c连接基类, 
class IConnection
{
    enum {recv_buff_length = 8192 };

public:
    IConnection(IServer* server, int conn_index);
    virtual ~IConnection();
    // thread-safe start from other thread
    virtual void Start();

    void AsyncSend(const char* data, int32 length);
    void AsyncSend(const boost::asio::const_buffer& buffer);
    void AsyncSend(const std::vector<boost::asio::const_buffer>& buffer);

    /// 
    void Close();
    void AsyncClose();

    boost::asio::ip::tcp::socket& socket() { return socket_;}
    int session_id() const { return session_id_; }
	std::string user_id() const { return user_id_; }
    void set_user_id(const std::string &user_id) {user_id_ = user_id;}

    boost::asio::io_service& io_service() const;

    // @return 0 means need more, positive means handled_size, negative means error
    virtual int32 OnRead(char* data, int32 length) = 0;
    // @param lenth: written length
    virtual void OnWrite(int32 length) {};
    /// all async operation end
    virtual void OnClosed() {};

protected:
    void StartRead();

    void HandleRead(const boost::system::error_code& ec, size_t length);

    void StartWrite();

    void HandleWrite(const boost::system::error_code& ec, size_t length);

    void HandleError(const boost::system::error_code& ec);

    void DoStart();

    void Destroy();

protected:
    IServer* server_;
    boost::asio::ip::tcp::socket socket_;

    boost::asio::deadline_timer deadline_timer_; 

    int session_id_;
	std::string user_id_;

    char read_data_[recv_buff_length];
    int32 read_size_;

    std::vector<boost::asio::const_buffer> send_queue_;
    std::vector<boost::asio::const_buffer> pending_send_queue_;

    std::atomic_uint op_count_;

    bool is_reading_;
    bool is_writing_;

    bool is_closing_;

    DISALLOW_COPY_AND_ASSIGN(IConnection);
};


} // namespace net
} // namespace z

#endif  // Z_NET_I_CONNECTION_H
