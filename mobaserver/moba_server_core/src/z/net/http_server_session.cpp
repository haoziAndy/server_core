#include "stdafx.h"
#include "http_server.h"
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <boost/config.hpp>
#include <algorithm>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include "http_server_session.h"


namespace z {
	namespace net {

			HServerSession::HServerSession(
				boost::asio::ip::tcp::socket socket, IHttpRequestHandler* msg_handler)
			: socket_(std::move(socket))
			, strand_(socket_.get_executor())
			, lambda_(*this)
		{
				msg_handler_ = msg_handler;
		}

		// Start the asynchronous operation
		void
			HServerSession::run()
		{
			do_read();
		}

		void
			HServerSession::do_read()
		{
			// Make the request empty before reading,
			// otherwise the operation behavior is undefined.
			req_ = {};

			// Read a request
			boost::beast::http::async_read(socket_, buffer_, req_,
				boost::asio::bind_executor(
					strand_,
					std::bind(
						&HServerSession::on_read,
						shared_from_this(),
						std::placeholders::_1,
						std::placeholders::_2)));
		}

		void
			HServerSession::on_read(
				boost::system::error_code ec,
				std::size_t bytes_transferred)
		{
			boost::ignore_unused(bytes_transferred);
			//std::cout << bytes_transferred << std::endl;
			// This means they closed the connection
			if (ec == boost::beast::http::error::end_of_stream)
				return do_close();

			if (ec)
			{
				LOG_ERR("Failed to read HServerSession,%s", ec.message().c_str());
				return do_close();
			}

			// Send the response
			handle_request(msg_handler_, std::move(req_), lambda_);
		}

		void
			HServerSession::on_write(
				boost::system::error_code ec,
				std::size_t bytes_transferred,
				bool close)
		{
			boost::ignore_unused(bytes_transferred);

			if (ec)
			{
				LOG_ERR("Failed to read HServerSession,%s", ec.message().c_str());
				return do_close();
			}

			if (close)
			{
				// This means we should close the connection, usually because
				// the response indicated the "Connection: close" semantic.
				return do_close();
			}

			// We're done with the response so delete it
			res_ = nullptr;

			// Read another request
			do_read();
		}

		void
			HServerSession::do_close()
		{
			// Send a TCP shutdown
			boost::system::error_code ec;
			socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_send, ec);

			// At this point the connection is closed gracefully
		}
	}
}