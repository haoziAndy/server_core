#ifndef _KCP_CONNECTION_HPP_
#define _KCP_CONNECTION_HPP_

#include <set>
#include <unordered_map>
#include <memory>
#include <boost/noncopyable.hpp>
#include <boost/asio.hpp>
#include "kcp_typedef.hpp"

namespace kcp_svr {

using namespace boost::asio::ip;
class connection_manager;

class connection
  : private boost::noncopyable, public std::enable_shared_from_this<connection>
{
public:
    typedef std::shared_ptr<connection> shared_ptr;
    typedef std::weak_ptr<connection> weak_ptr;

    connection(const std::weak_ptr<connection_manager>& manager_ptr);
    ~connection(void);

    static connection::shared_ptr create(const std::weak_ptr<connection_manager>& manager_ptr,
            const kcp_conv_t& conv, const udp::endpoint& udp_remote_endpoint);

    void set_udp_remote_endpoint(const udp::endpoint& udp_remote_endpoint);
	const udp::endpoint  get_udp_remote_endpoint() const;

    // changing udp_remote_endpoint at every packet. Because we allow connection change ip or port. we using conv to indicate a connection.
    void input(char* udp_data, size_t bytes_recvd, const udp::endpoint& udp_remote_endpoint);

    void update_kcp(uint32_t clock);

    bool is_timeout(void) const;
    void do_timeout(void);

    // user level send msg.
    void send_kcp_msg(const std::string& msg);
	void send_kcp_msg(char *msg, const int length);

    // todo need close if connection bind some asio callback.
    //void close();

	void set_session_id(const int32 session_id) { session_id_ = session_id; };
	int32 session_id() const { return session_id_; };

	bool check_sndwnd() const;
	 
private:
    void init_kcp(const kcp_conv_t& conv);
    void clean(void);
    static int udp_output(const char *buf, int len, ikcpcb *kcp, void *user);
    void send_udp_package(const char *buf, int len);

    uint32_t get_cur_clock(void) const;
    uint32_t get_timeout_time(void) const;
	void async_send_udp_package(const char* data, const int length);
	void start_write();
	void handle_async_write(const boost::system::error_code& ec, size_t bytes_transferred);
	void on_write(const int32 length);

private:
    std::weak_ptr<connection_manager> connection_manager_weak_ptr_; // -known
    kcp_conv_t conv_;
    ikcpcb* p_kcp_; // --own
    udp::endpoint udp_remote_endpoint_;
    uint32_t last_packet_recv_time_;
	int32 session_id_;
	bool is_writing_;
	bool is_closing_;
	std::vector<boost::asio::const_buffer> send_queue_;
	std::vector<boost::asio::const_buffer> pending_send_queue_;
};

} // namespace kcp_svr

#endif // _KCP_CONNECTION_HPP_
