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
#include "http_server_session.h"
#include "http_server.h"
#include "z_server.h"


namespace z {
	namespace net {


			HServerSession::HServerSession(
				boost::asio::ip::tcp::socket&& socket,
				IHttpRequestHandler* msg_handler,const uint32 session_id)
				: stream_(std::move(socket))
				, send_lambda_(*this)
				, session_id_(session_id)
			{
				msg_handler_ = msg_handler;
				start_handle_req_ts_ = 0;
			}

		// Start the asynchronous operation
		void
			HServerSession::run()
		{
			boost::asio::dispatch(stream_.get_executor(),
				boost::beast::bind_front_handler(
					&HServerSession::do_read,
					shared_from_this()));
		}

		void
			HServerSession::do_read()
		{
			// Make the request empty before reading,
			// otherwise the operation behavior is undefined.
			req_ = {};

			start_handle_req_ts_ = 0;
			// Set the timeout.
			stream_.expires_after(std::chrono::seconds(30));

			// Read a request
			boost::beast::http::async_read(stream_, buffer_, req_,
				boost::beast::bind_front_handler(
					&HServerSession::on_read,
					shared_from_this()));
		}

		void
			HServerSession::on_read(
				boost::beast::error_code ec,
				std::size_t bytes_transferred)
		{
			boost::ignore_unused(bytes_transferred);
			//std::cout << bytes_transferred << std::endl;
			// This means they closed the connection
			if (ec == boost::beast::http::error::end_of_stream)
				return do_close();

			if (ec)
			{
				LOG_DEBUG("Failed to read HServerSession,%s", ec.message().c_str());
				return do_close();
			}

			request_version_ = req_.version();
			request_keep_alive_ = req_.keep_alive();
			handle_request(msg_handler_, std::move(req_), this->send_lambda_,this->session_id_);
			start_handle_req_ts_ = TIME_ENGINE.time_sec();
		}

		void
			HServerSession::on_write(
				bool close,
				boost::beast::error_code ec,
				std::size_t bytes_transferred)
		{
			boost::ignore_unused(bytes_transferred);

			if (ec)
			{
				LOG_DEBUG("Failed to write HServerSession,%s", ec.message().c_str());
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
			boost::beast::error_code ec;
			stream_.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_send, ec);

			// At this point the connection is closed gracefully

			HSERVER.remove_session(session_id_);
		}

		void HServerSession::response(const std::string &content)
		{
			boost::beast::http::string_body::value_type body(content);
			auto const size = body.size();
			// Respond to GET request
			boost::beast::http::response<boost::beast::http::string_body> res{
				std::piecewise_construct,
				std::make_tuple(std::move(body)),
				std::make_tuple(boost::beast::http::status::ok,request_version_ ) };
			res.set(boost::beast::http::field::server, BOOST_BEAST_VERSION_STRING);
			res.set(boost::beast::http::field::content_type, "application/json");
			res.content_length(size);
			res.keep_alive(request_keep_alive_);
			this->send_lambda_(std::move(res));
		}

		bool HServerSession::handle_req_timeout()
		{
			if (start_handle_req_ts_ == 0)
			{
				return false;
			}
			const uint32 ts = TIME_ENGINE.time_sec();
			return (ts - start_handle_req_ts_) >= 30;
		}
	}
}