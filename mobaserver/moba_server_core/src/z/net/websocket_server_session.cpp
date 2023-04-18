#include "stdafx.h"
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
#include "websocket_server_session.h"
#include "websocket_server.h"
#include "z_server.h"


namespace z {
	namespace net {


		WSServerSession::WSServerSession(
			boost::asio::ip::tcp::socket&& socket,
			IHttpRequestHandler* msg_handler, const uint32 session_id)
			: ws_(std::move(socket))
			, session_id_(session_id)
		{
			msg_handler_ = msg_handler;
		}

		// Start the asynchronous operation
		void WSServerSession::run()
		{
			// We need to be executing within a strand to perform async operations
			// on the I/O objects in this session. Although not strictly necessary
			// for single-threaded contexts, this example code is written to be
			// thread-safe by default.
			boost::beast::net::dispatch(ws_.get_executor(),
				boost::beast::bind_front_handler(
					&WSServerSession::on_run,
					shared_from_this()));
		}

		// Start the asynchronous operation
		void WSServerSession::on_run()
		{
			// Set suggested timeout settings for the websocket
			ws_.set_option(
				boost::beast::websocket::stream_base::timeout::suggested(
					boost::beast::role_type::server));

			// Set a decorator to change the Server of the handshake
			ws_.set_option(boost::beast::websocket::stream_base::decorator(
				[](boost::beast::websocket::response_type& res)
			{
				res.set(boost::beast::http::field::server,
					std::string(BOOST_BEAST_VERSION_STRING) +
					" websocket-server-async");
			}));
			// Accept the websocket handshake
			ws_.async_accept(
				boost::beast::bind_front_handler(
					&WSServerSession::on_accept,
					shared_from_this()));
		}

		void WSServerSession::on_accept(boost::beast::error_code ec)
		{
			if (ec)
			{
				//remove
				//return fail(ec, "accept");
			}
				

			// Read a message
			do_read();
		}

		void WSServerSession::do_read()
		{
			// Read a message into our buffer
			ws_.async_read(
				buffer_,
				boost::beast::bind_front_handler(
					&WSServerSession::on_read,
					shared_from_this()));
		}


		void WSServerSession::on_read(
				boost::beast::error_code ec,
				std::size_t bytes_transferred)
		{
			boost::ignore_unused(bytes_transferred);

			// This indicates that the session was closed
			if (ec == boost::beast::websocket::error::closed)
			{
				//remove
				return;
			}

			if (ec)
			{
				//remove
			}
		}


	}
}