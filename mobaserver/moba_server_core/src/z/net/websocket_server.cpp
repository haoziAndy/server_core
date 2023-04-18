#include "stdafx.h"
#include "websocket_server_session.h"
#include "msg_handler.h"
#include "websocket_server.h"
#include "z_server.h"


namespace z {
	namespace net {

		WSServer::WSServer() : ioc_(TIME_ENGINE)
			, acceptor_(boost::asio::make_strand(ioc_))
			, deadline_timer_(TIME_ENGINE)
		{
		}

		bool WSServer::Init(boost::asio::ip::tcp::endpoint endpoint, IHttpRequestHandler* msg_handler)
		{
			if (msg_handler == nullptr) {
				LOG_FATAL(" IHttpRequestHandler is null ");
				return false;
			}

			msg_handler_ = msg_handler;
			boost::beast::error_code ec;

			// Open the acceptor
			acceptor_.open(endpoint.protocol(), ec);
			if (ec)
			{
				LOG_FATAL("Failed to init WSServer,%s", ec.message().c_str());
				return false;
			}

			// Allow address reuse
			acceptor_.set_option(boost::beast::net::socket_base::reuse_address(true), ec);
			if (ec)
			{
				LOG_FATAL("Failed to set_option WSServer,%s", ec.message().c_str());
				return false;
			}

			// Bind to the server address
			acceptor_.bind(endpoint, ec);
			if (ec)
			{
				LOG_FATAL("Failed to bind WSServer,%s", ec.message().c_str());
				return false;
			}

			// Start listening for connections
			acceptor_.listen(
				boost::beast::net::socket_base::max_listen_connections, ec);
			if (ec)
			{
				LOG_FATAL("Failed to listen WSServer,%s", ec.message().c_str());
				return false;
			}

			LOG_INFO("Success to start websocket server");
			return true;
		}

		void WSServer::run()
		{
			do_accept();
		}

		void WSServer::do_accept()
		{
			// The new connection gets its own strand
			acceptor_.async_accept(
				boost::beast::net::make_strand(ioc_),
				boost::beast::bind_front_handler(
					&WSServer::on_accept,
					this));
		}

		void WSServer::on_accept(boost::beast::error_code ec, boost::asio::ip::tcp::socket socket)
		{
			if (ec)
			{
				LOG_FATAL("Failed to Accept,%s", ec.message().c_str());
			}
			else
			{
				// Create the session and run it
				//std::make_shared<session>(std::move(socket))->run();
			}

			// Accept another connection
			do_accept();
		}

	}
}