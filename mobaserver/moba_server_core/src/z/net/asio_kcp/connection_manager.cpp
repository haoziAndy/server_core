#include "stdafx.h"
#include "connection_manager.hpp"
#include <algorithm>

#include <cstdlib>
#include <iostream>
#ifndef _UNISTD_H
#define _UNISTD_H
#include <io.h>
#include <process.h>
#else
	#include <unistd.h>
#endif /* _UNISTD_H */

#ifdef WIN32
#include <windows.h>
#else
#  include <sys/time.h>
#endif

#include <ikcp.h>
#include "connect_packet.hpp"

/* get system time */
static inline void itimeofday(long *sec, long *usec)
{
#if defined(__unix)
	struct timeval time;
	gettimeofday(&time, NULL);
	if (sec) *sec = time.tv_sec;
	if (usec) *usec = time.tv_usec;
#else
	static long mode = 0, addsec = 0;
	BOOL retval;
	static IINT64 freq = 1;
	IINT64 qpc;
	if (mode == 0) {
		retval = QueryPerformanceFrequency((LARGE_INTEGER*)&freq);
		freq = (freq == 0) ? 1 : freq;
		retval = QueryPerformanceCounter((LARGE_INTEGER*)&qpc);
		addsec = (long)time(NULL);
		addsec = addsec - (long)((qpc / freq) & 0x7fffffff);
		mode = 1;
	}
	retval = QueryPerformanceCounter((LARGE_INTEGER*)&qpc);
	retval = retval * 2;
	if (sec) *sec = (long)(qpc / freq) + addsec;
	if (usec) *usec = (long)((qpc % freq) * 1000000 / freq);
#endif
}

/* get clock in millisecond 64 */
static inline uint64_t iclock64(void)
{
    long s, u;
    uint64_t value;
    itimeofday(&s, &u);
    value = ((uint64_t)s) * 1000 + (u / 1000);
    return value;
}


static inline uint32_t iclock()
{
    return (uint32_t)(iclock64() & 0xfffffffful);
}


namespace kcp_svr {

uint64_t endpoint_to_i(const udp::endpoint& ep)
{
    uint64_t addr_i = ep.address().to_v4().to_ulong();
    uint32_t port = ep.port();
    return (addr_i << 32) + port;
}


connection_manager::connection_manager(boost::asio::io_service& io_service) :
    stopped_(false),
    udp_socket_(io_service),
    kcp_timer_(io_service),
    cur_clock_(0)
{
}

bool connection_manager::connection_manager_init( const std::string& address, int udp_port)
{
	boost::asio::ip::udp::resolver resolver(udp_socket_.get_executor());
	boost::asio::ip::udp::resolver::query query(address, std::to_string(udp_port));

	boost::system::error_code ec;
	auto it = resolver.resolve(query, ec);
	if (ec)
	{
		LOG_ERR("connection_manager::connection_manager failed. Error[%d]: %s", ec.value(), ec.message().c_str());
		return false;
	}
	boost::asio::ip::udp::endpoint endpoint = *it;

	udp_socket_.open(endpoint.protocol());
	udp_socket_.bind(endpoint, ec);
	if (ec)
	{
		LOG_ERR("connection_manager::connection_manager failed. Error[%d]: %s", ec.value(), ec.message().c_str());
		return false;
	}

	//udp_socket_.set_option(udp::socket::non_blocking_io(false)); // why this make compile fail

	hook_udp_async_receive();
	hook_kcp_timer();
	LOG_INFO("Success to init kcp server");
	return true;
}

void connection_manager::stop_all()
{
  stopped_ = true;
  connections_.stop_all();

  udp_socket_.cancel();
  udp_socket_.close();
}

void connection_manager::force_disconnect(const kcp_conv_t& conv)
{
    if (!connections_.find_by_conv(conv))
        return;

   /* std::shared_ptr<std::string> msg(new std::string("server force disconnect"));
    call_event_callback_func(conv, eEventType::eDisconnect, msg);*/
    connections_.remove_connection(conv);
}

void connection_manager::set_callback(const std::function<event_callback_t>& func)
{
    event_callback_ = func;
}

void connection_manager::call_event_callback_func(kcp_conv_t conv, eEventType event_type, std::shared_ptr<std::string> msg)
{
   // event_callback_(conv, event_type, msg);
}

void connection_manager::handle_connect_packet()
{
    kcp_conv_t conv = connections_.get_new_conv();
    std::string send_back_msg = asio_kcp::making_send_back_conv_packet(conv);
    udp_socket_.send_to(boost::asio::buffer(send_back_msg), udp_remote_endpoint_);
    connections_.add_new_connection(shared_from_this(), conv, udp_remote_endpoint_);
}

void connection_manager::handle_kcp_packet(size_t bytes_recvd)
{
	//ikcp.c IKCP_OVERHEAD  24
	if (bytes_recvd < 24)
	{
		return;
	}
	const IUINT32 conv = ikcp_getconv(udp_data_);
    if (conv <= 0)
    {
		LOG_DEBUG("ikcp_get_conv return 0");
        return;
    }

    connection::shared_ptr conn_ptr = connections_.find_by_conv(conv);
    if (!conn_ptr)
    {
		LOG_DEBUG ("connection not exist with conv: %d",  conv );
        return;
    }

    if (conn_ptr)
        conn_ptr->input(udp_data_, bytes_recvd, udp_remote_endpoint_);
    else
       LOG_ERR( "add_new_connection failed! can not connect!");
}

void connection_manager::handle_udp_receive_from(const boost::system::error_code& error, size_t bytes_recvd)
{
    if (!error && bytes_recvd > 0)
    {
        /*
        std::cout << "\nudp_sender_endpoint: " << udp_remote_endpoint_ << std::endl;
        unsigned long addr_i = udp_remote_endpoint_.address().to_v4().to_ulong();
        std::cout << addr_i << " " << udp_remote_endpoint_.port() << std::endl;
        std::cout << "udp recv: " << bytes_recvd << std::endl <<
            Essential::ToHexDumpText(std::string(udp_data_, bytes_recvd), 32) << std::endl;
        */

        #if AK_ENABLE_UDP_PACKET_LOG
		AK_UDP_PACKET_LOG << "udp_recv:" << udp_remote_endpoint_.address().to_string() << ":" << udp_remote_endpoint_.port()
			<< " conv:" << 0
			<< " size:" << bytes_recvd << "\n"
			<< Essential::ToHexDumpText(std::string(udp_data_, bytes_recvd), 32);
        #endif

		//LOG_DEBUG("AAA = rev = %s need = %d ", std::string(udp_data_, bytes_recvd).c_str(), sizeof("asio_kcp_connect_package get_conv"));
        if (asio_kcp::is_connect_packet(udp_data_, bytes_recvd))
        {
            handle_connect_packet();
            goto END;
        }

        handle_kcp_packet(bytes_recvd);
    }
    else
    {
		LOG_DEBUG("\nhandle_udp_receive_from error end! error: %s, bytes_recvd: %ld\n", error.message().c_str(), bytes_recvd);
		
    }

END:
    hook_udp_async_receive();
}

void connection_manager::hook_udp_async_receive(void)
{
    if (stopped_)
        return;
    udp_socket_.async_receive_from(
          boost::asio::buffer(udp_data_, sizeof(udp_data_)), udp_remote_endpoint_,
          boost::bind(&connection_manager::handle_udp_receive_from, shared_from_this(),
              boost::asio::placeholders::error,
              boost::asio::placeholders::bytes_transferred));
}

void connection_manager::hook_kcp_timer(void)
{
    if (stopped_)
        return;
    kcp_timer_.expires_from_now(boost::posix_time::milliseconds(5));
	kcp_timer_.async_wait(boost::bind(&connection_manager::handle_kcp_time, shared_from_this(), boost::asio::placeholders::error));
	
}

void connection_manager::handle_kcp_time(const boost::system::error_code& ec)
{
    //std::cout << "."; std::cout.flush();
    hook_kcp_timer();
    cur_clock_ = iclock();
    connections_.update_all_kcp(cur_clock_);
}

void connection_manager::send_udp_packet(const std::string& msg, const boost::asio::ip::udp::endpoint& endpoint)
{
    udp_socket_.send_to(boost::asio::buffer(msg), endpoint);
}

int connection_manager::send_msg(const kcp_conv_t& conv, std::shared_ptr<std::string> msg)
{
    connection::shared_ptr connection_ptr = connections_.find_by_conv(conv);
    if (!connection_ptr)
        return -1;

	if (!connection_ptr->check_sndwnd())
	{
		UDPSERVER.CloseConnection(connection_ptr->session_id());
		return -1;
	}
    connection_ptr->send_kcp_msg(*msg);
    return 0;
}

int connection_manager::send_msg(const kcp_conv_t& conv, char * msg,const int32 length)
{
	connection::shared_ptr connection_ptr = connections_.find_by_conv(conv);
	if (!connection_ptr)
		return -1;

	if (!connection_ptr->check_sndwnd())
	{
		UDPSERVER.CloseConnection(connection_ptr->session_id());
		return -1;
	}

	connection_ptr->send_kcp_msg(msg, length);
	return 0;
}

udp::socket* connection_manager::get_udp_socket() {
	return &udp_socket_;
}

} // namespace kcp_svr
