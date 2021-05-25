#include "stdafx.h"
#include "http_server_session.h"
#include "msg_handler.h"
#include "http_server.h"
#include "z_server.h"

namespace z {
	namespace net {

		HServer::HServer() : ioc_(TIME_ENGINE)
			, acceptor_(boost::asio::make_strand(ioc_))
			, deadline_timer_(TIME_ENGINE)
		{
			
		}

		bool HServer::Init(boost::asio::ip::tcp::endpoint endpoint, IHttpRequestHandler* msg_handler)
		{
			if (msg_handler == nullptr){
				LOG_FATAL(" IHttpRequestHandler is null ");
				return false;
			}

			msg_handler_ = msg_handler;
			boost::beast::error_code ec;

			// Open the acceptor
			acceptor_.open(endpoint.protocol(), ec);
			if (ec)
			{
				LOG_FATAL("Failed to init HServer,%s", ec.message().c_str());
				return false;
			}

			// Allow address reuse
			acceptor_.set_option(boost::asio::socket_base::reuse_address(true), ec);
			if (ec)
			{
				LOG_FATAL("Failed to init HServer,%s", ec.message().c_str());
				return false;
			}

			// Bind to the server address
			acceptor_.bind(endpoint, ec);
			if (ec)
			{
				LOG_FATAL("Failed to init HServer,%s", ec.message().c_str());
				return false;
			}

			// Start listening for connections
			acceptor_.listen(
				boost::asio::socket_base::max_listen_connections, ec);
			if (ec)
			{
				LOG_FATAL("Failed to init HServer,%s", ec.message().c_str());
				return false;
			}
			if (!acceptor_.is_open())
			{
				LOG_FATAL("HServer acceptor is not opend");
				return false;
			}
			LOG_INFO("Success to start http server");
			return true;
		}

		// Start accepting incoming connections
		void
			HServer::run()
		{
			if (!acceptor_.is_open())
			{
				LOG_FATAL("HServer acceptor is not opend");
				return;
			}
			deadline_timer_.expires_from_now(boost::posix_time::seconds(30));
			deadline_timer_.async_wait(boost::bind(&HServer::CheckHandleReqTimeout, this, boost::asio::placeholders::error));
			do_accept();
		}

		void
			HServer::do_accept()
		{
			acceptor_.async_accept(
				boost::asio::make_strand(ioc_),
				boost::beast::bind_front_handler(
					&HServer::on_accept,
					this));
		}

		void
			HServer::on_accept(boost::beast::error_code ec, boost::asio::ip::tcp::socket socket)
		{
			if (ec)
			{
				LOG_FATAL("Failed to Accept,%s", ec.message().c_str());
			}
			else
			{
				// Create the session and run it
				static int h_s_conn_index = 0;
				
				auto conn_index = ((++h_s_conn_index) << 1) | 0x1;

				// 42亿个id, 应该不会被占满, 做个保护, 最大循环100
				for (int i = 0; connection_mgr_.find(conn_index) != connection_mgr_.end() && i < 100; ++i)
					++conn_index;

				if (connection_mgr_.find(conn_index) == connection_mgr_.end()) {
					std::shared_ptr<HServerSession> new_session = std::make_shared<HServerSession>(
						std::move(socket), msg_handler_, conn_index);
					connection_mgr_[conn_index] = new_session;

					new_session->run();
				}
				else
				{
					LOG_FATAL("Http session dumplicate,Oh No ");
				}

			}

			// Accept another connection
			do_accept();
		}

		void
			HServer::stop()
		{
			acceptor_.close();
		}

		void HServer::remove_session(const uint32 http_session_id)
		{
			auto it = connection_mgr_.find(http_session_id);
			if (it != connection_mgr_.end())
			{
				connection_mgr_.erase(it);
			}
		}
		void HServer::response(const uint32 http_session_id,const std::string& content)
		{
			auto it = connection_mgr_.find(http_session_id);
			if (it == connection_mgr_.end())
			{
				LOG_ERR("http_session_id = %d lose efficacy ", http_session_id);
				return;
			}
			if (it->second == nullptr)
			{
				LOG_FATAL("http_session_id = %d ptr == null ", http_session_id);
				return;
			}
			it->second->response(content);
		}

		void HServer::CheckHandleReqTimeout(const boost::system::error_code& ec)
		{
			if (!ec)
			{
				std::vector<uint32> timeout_list;
				for (auto it = connection_mgr_.begin(); it != connection_mgr_.end(); ++it)
				{
					if (it->second != nullptr)
					{
						if (it->second->handle_req_timeout())
						{
							timeout_list.push_back(it->first);
						}
					}
				}
				for (auto it = timeout_list.begin(); it != timeout_list.end(); ++it)
				{
					connection_mgr_[*it]->do_close();
				}
			}
			deadline_timer_.expires_from_now(boost::posix_time::seconds(30));
			deadline_timer_.async_wait(boost::bind(&HServer::CheckHandleReqTimeout, this, boost::asio::placeholders::error));
		}
	}
}