#include "stdafx.h"
#include "http_server.h"
#include "http_server_session.h"
#include "msg_handler.h"
#include "z_server.h"

namespace z {
	namespace net {

		HServer::HServer(boost::asio::io_context& ioc) : ioc_(ioc)
			, acceptor_(boost::asio::make_strand(ioc))
		{
			
		}

		bool HServer::Init(boost::asio::ip::tcp::endpoint endpoint, IHttpRequestHandler* msg_handler)
		{
			if (msg_handler == nullptr){
				LOG_FATAL(" IHttpRequestHandler is null ");
				return false;
			}

			msg_handler_ = msg_handler;
			boost::system::error_code ec;

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
			do_accept();
		}

		void
			HServer::do_accept()
		{
			acceptor_.async_accept(
				boost::asio::make_strand(ioc_),
				boost::beast::bind_front_handler(
					&HServer::on_accept,
					shared_from_this()));
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
				std::make_shared<HServerSession>(
					std::move(socket), msg_handler_)->run();
			}

			// Accept another connection
			do_accept();
		}

		void
			HServer::stop()
		{
			acceptor_.close();
		}
	}
}