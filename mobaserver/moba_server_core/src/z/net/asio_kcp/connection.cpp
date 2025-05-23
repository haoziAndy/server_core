#include "stdafx.h"
#include "connection.hpp"
#include <algorithm>

#include "ikcp.h"
#include "connect_packet.hpp"
#include <cstdlib>
#include <iostream>
#include "connection_manager.hpp"


namespace kcp_svr {

	//using namespace boost::asio::ip;

	connection::connection(const std::weak_ptr<connection_manager>& manager_ptr) :
		connection_manager_weak_ptr_(manager_ptr),
		conv_(0),
		p_kcp_(NULL),
		last_packet_recv_time_(0),
		is_writing_(false),
		is_closing_(false),
		pending_send_queue_(send_queue_)
	{
	}

	connection::~connection(void)
	{
		clean();
	}

	void connection::clean(void)
	{
		LOG_DEBUG("clean connection conv: %d ", conv_);
		is_closing_ = true;
		if (auto ptr = connection_manager_weak_ptr_.lock())
		{
			std::string disconnect_msg = asio_kcp::making_disconnect_packet(conv_);
			ptr->send_udp_packet(disconnect_msg, udp_remote_endpoint_);
		}
		ikcp_release(p_kcp_);
		p_kcp_ = NULL;
		conv_ = 0;

		for (auto it = send_queue_.begin(); it != send_queue_.end(); ++it)
		{
			auto& buffer = *it;
			const char* data = boost::asio::buffer_cast<const char*>(buffer);
			ZPOOL_FREE(static_cast<void*>(const_cast<char*>(data)));
		}
		send_queue_.clear();

		for (auto it = pending_send_queue_.begin(); it != pending_send_queue_.end(); ++it)
		{
			auto& buffer = *it;
			const char* data = boost::asio::buffer_cast<const char*>(buffer);
			ZPOOL_FREE(static_cast<void*>(const_cast<char*>(data)));
		}
		pending_send_queue_.clear();
	}

	connection::shared_ptr connection::create(const std::weak_ptr<connection_manager>& manager_ptr,
		const kcp_conv_t& conv, const udp::endpoint& udp_remote_endpoint)
	{
		shared_ptr ptr = std::make_shared<connection>(manager_ptr);
		if (ptr)
		{
			ptr->init_kcp(conv);
			ptr->set_udp_remote_endpoint(udp_remote_endpoint);
			LOG_DEBUG("New UDP connection from: ip = %s,port = %d,conv = %u ", udp_remote_endpoint.address().to_string().c_str(), udp_remote_endpoint.port(), conv);
			//   AK_INFO_LOG << "new connection from: " << udp_remote_endpoint;
		}
		return ptr;
	}

	void connection::set_udp_remote_endpoint(const udp::endpoint& udp_remote_endpoint)
	{
		udp_remote_endpoint_ = udp_remote_endpoint;
	}

	const udp::endpoint connection::get_udp_remote_endpoint() const {
		return udp_remote_endpoint_;
	}

	void connection::init_kcp(const kcp_conv_t& conv)
	{
		conv_ = conv;
		p_kcp_ = ikcp_create(conv, (void*)this);
		p_kcp_->output = &connection::udp_output;

		// 启动快速模式
		// 第二个参数 nodelay-启用以后若干常规加速将启动
		// 第三个参数 interval为内部处理时钟，默认设置为 10ms
		// 第四个参数 resend为快速重传指标，设置为2
		// 第五个参数 为是否禁用常规流控，这里禁止
		//ikcp_nodelay(p_kcp_, 1, 10, 2, 1);
		ikcp_nodelay(p_kcp_, 1,5,2,1); // 设置成1次ACK跨越直接重传, 这样反应速度会更快. 内部时钟5毫秒.

		ikcp_wndsize(p_kcp_, ASIO_KCP_FLAGS_SNDWND, ASIO_KCP_FLAGS_RCVWND);

		p_kcp_->rx_minrto = 10;
	}

	// 发送一个 udp包
	int connection::udp_output(const char *buf, int len, ikcpcb *kcp, void *user)
	{
		((connection*)user)->send_udp_package(buf, len);
		return 0;
	}

	void connection::send_udp_package(const char *buf, int len)
	{
		/*if (auto ptr = connection_manager_weak_ptr_.lock())
		{
			ptr->send_udp_packet(std::string(buf, len), udp_remote_endpoint_);
		}*/
		async_send_udp_package(buf, len);
	}

	void connection::send_kcp_msg(const std::string& msg)
	{
		int send_ret = ikcp_send(p_kcp_, msg.c_str(), msg.size());
		if (send_ret < 0)
		{
			LOG_ERR( "send_ret<0: %d",  send_ret);
		}
		else {
			ikcp_flush(p_kcp_);
		}
	}

	void connection::send_kcp_msg(char *msg,const int length)
	{
		int send_ret = ikcp_send(p_kcp_, msg, length);
		if (send_ret < 0)
		{
			LOG_ERR( "send_ret<0: %d", send_ret);
		}
		else {
			ikcp_flush(p_kcp_);
		}
	}

	void connection::input(char* udp_data, size_t bytes_recvd, const udp::endpoint& udp_remote_endpoint)
	{
		last_packet_recv_time_ = get_cur_clock();
		udp_remote_endpoint_ = udp_remote_endpoint;
		
		ikcp_input(p_kcp_, udp_data, bytes_recvd);
	

		/* todo need re recv or not?
		while (true)
		{
		char kcp_buf[1024 * 1000] = "";
		int kcp_recvd_bytes = ikcp_recv(p_kcp_, kcp_buf, sizeof(kcp_buf));
		if (kcp_recvd_bytes <= 0)
		{
		std::cout << "\nkcp_recvd_bytes<=0: " << kcp_recvd_bytes << std::endl;
		break;
		}
		const std::string package(kcp_buf, kcp_recvd_bytes);
		std::cout << "\nkcp recv: " << kcp_recvd_bytes << std::endl << Essential::ToHexDumpText(package, 32) << std::endl;

		ikcp_input(p_kcp_, "", 0);
		}
		*/

		{
			static const int32 kcp_rev_buff_len = 1024 * 10;
			//char kcp_buf[1024 * 1000] = "";
			char* kcp_buf = reinterpret_cast<char*>(ZPOOL_MALLOC(kcp_rev_buff_len));
			int kcp_recvd_bytes = ikcp_recv(p_kcp_, kcp_buf, kcp_rev_buff_len/*sizeof(kcp_buf)*/);
			if (kcp_recvd_bytes <= 0)
			{
				if (kcp_recvd_bytes == -1) {

				}
				else {
					LOG_DEBUG("kcp_recvd_bytes<=0: %d", kcp_recvd_bytes);
				}
			}
			else
			{
				auto session = UDPSERVER.GetConnection(this->session_id());
				if (session){
					const int32 res = session->OnRead(kcp_buf, kcp_recvd_bytes);
					if ( res < 0) {
						UDPSERVER.CloseConnection(this->session_id());
					}
				}
				/*std::string package(kcp_buf, kcp_recvd_bytes);
				if (auto ptr = connection_manager_weak_ptr_.lock())
				{
					LOG_DEBUG("packagepackagepackage = %s", package.c_str());
					send_kcp_msg(package);
					//ptr->call_event_callback_func(conv_, eRcvMsg, std::make_shared<std::string>(package));
				}*/
			}
			ZPOOL_FREE(kcp_buf);
		}

	}

	void connection::update_kcp(uint32_t clock)
	{
		ikcp_update(p_kcp_, clock);
	}


	bool connection::is_timeout(void) const
	{
		if (last_packet_recv_time_ == 0)
			return false;

		return get_cur_clock() - last_packet_recv_time_ > get_timeout_time();
	}

	void connection::do_timeout(void)
	{
		/*if (auto ptr = connection_manager_weak_ptr_.lock())
		{
			std::shared_ptr<std::string> msg(new std::string("timeout"));
			ptr->call_event_callback_func(conv_, eEventType::eDisconnect, msg);
		}*/
	}

	uint32_t connection::get_timeout_time(void) const
	{
		return UDPSERVER.keepalive_time_out_sec() * 1000;
	}

	uint32_t connection::get_cur_clock(void) const
	{
		if (auto ptr = connection_manager_weak_ptr_.lock())
		{
			return ptr->get_cur_clock();
		}
		return 0;
	}

	bool connection::check_sndwnd() const
	{
		const int waitsnd = ikcp_waitsnd(p_kcp_);
		if (waitsnd > ASIO_KCP_FLAGS_SNDWND * 2) {
			LOG_INFO(" waitsnd = %d > %d * 2 ", waitsnd, ASIO_KCP_FLAGS_SNDWND);
			return false;
		}
		return true;
	}

	void connection::async_send_udp_package(const char* data, const int length)
	{
		char* buff = reinterpret_cast<char*>(ZPOOL_MALLOC(length));
		memcpy(buff, data, length);
		if (!is_writing_)
		{
			send_queue_.push_back(boost::asio::buffer(buff, length));
			start_write();
		}
		else
		{
			enum { max_pending_send_size = 256 };
			// 增加保护, 防止不停的累加
			if (pending_send_queue_.size() >= max_pending_send_size)
			{
				LOG_INFO("udp pending_send_queue_ too large %d", static_cast<int>(pending_send_queue_.size()));
				UDPSERVER.CloseConnection(this->session_id());
				return;
			}
			pending_send_queue_.push_back(boost::asio::buffer(buff, length));
		}
	}

	void connection::start_write()
	{
		if (is_closing_)
			return;

		if (is_writing_)
			return;

		if (send_queue_.empty())
			return;

		is_writing_ = true;

		if (auto ptr = connection_manager_weak_ptr_.lock())
		{
			auto udp_socket = ptr->get_udp_socket();
			udp_socket->async_send_to(send_queue_, udp_remote_endpoint_,
				boost::bind(&connection::handle_async_write, shared_from_this(),
					boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
		}
	
	}

	void connection::handle_async_write(const boost::system::error_code& ec, size_t bytes_transferred)
	{
		is_writing_ = false;

		//if (is_closing_)
		//	return;

		if (!ec)
		{
			on_write(bytes_transferred);
			if (!pending_send_queue_.empty())
			{
				send_queue_.swap(pending_send_queue_);
				// continue write
				start_write();
			}
		}
		else
		{
			if (ec == boost::asio::error::operation_aborted)
				return;

			if (!(ec == boost::asio::error::connection_reset ||
				ec == boost::asio::error::eof))
			{
				LOG_DEBUG("udp session[%d] error[%d]:%s", session_id(), ec.value(), ec.message().c_str());
			}
			UDPSERVER.CloseConnection(this->session_id());
		}
	}

	void connection::on_write(const int32 length)
	{
		for (auto it = send_queue_.begin(); it != send_queue_.end(); ++it)
		{
			auto& buffer = *it;
			const char* data = boost::asio::buffer_cast<const char*>(buffer);
			ZPOOL_FREE(static_cast<void*>(const_cast<char*>(data)));
		}
		send_queue_.clear();
	}

} // namespace kcp_svr
