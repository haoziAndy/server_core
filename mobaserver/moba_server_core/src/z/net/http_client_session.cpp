#include "stdafx.h"
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include "http_client_session.h"
#include "z_server.h"


namespace z {
	namespace net {
		// Start the asynchronous operation
		void HttpClientSession::run(
				char const* host,
				char const* port,
				char const* target,
				const std::string &body,
				const std::string &content_type,
				const int version,
			    std::function<void(const std::string&, bool)> callback)
		{
			// Set up an HTTP GET request message
			req_.version(version);
			req_.method(boost::beast::http::verb::post);
			req_.target(target);
			req_.set(boost::beast::http::field::host, host);
			req_.set(boost::beast::http::field::user_agent, BOOST_BEAST_VERSION_STRING);
			req_.set(boost::beast::http::field::content_type, content_type);
			req_.body() = body;
			req_.prepare_payload();
			res_callback_ = callback;
			// Look up the domain name
			resolver_.async_resolve(
				host,
				port,
				boost::beast::bind_front_handler(
					&HttpClientSession::on_resolve,
					shared_from_this()));
		}

		void
			HttpClientSession::on_resolve(
				boost::beast::error_code ec,
				boost::asio::ip::tcp::resolver::results_type results)
		{
			if (ec)
			{
				LOG_ERR("http resolve: %s", ec.message().c_str());
				if (res_callback_ != nullptr) {
					res_callback_("", false);
				}
				return;
			}
			const uint32 timeout_ = ZSERVER.get_http_client_timeout_ts();
			// Set a timeout on the operation
			stream_.expires_after(std::chrono::seconds(timeout_));
			// Make the connection on the IP address we get from a lookup
			stream_.async_connect(
				results,
				boost::beast::bind_front_handler(
					&HttpClientSession::on_connect,
					shared_from_this()));
		}

		void
			HttpClientSession::on_connect(boost::beast::error_code ec, boost::asio::ip::tcp::resolver::results_type::endpoint_type)
		{
			if (ec)
			{
				LOG_ERR("http connect: %s", ec.message().c_str());
				if (res_callback_ != nullptr) {
					res_callback_("", false);
				}
				return;
			}
			const uint32 timeout_ = ZSERVER.get_http_client_timeout_ts();
			// Set a timeout on the operation
			stream_.expires_after(std::chrono::seconds(timeout_));

			// Send the HTTP request to the remote host
			boost::beast::http::async_write(stream_, req_,
				boost::beast::bind_front_handler(
					&HttpClientSession::on_write,
					shared_from_this()));
		}

		void
			HttpClientSession::on_write(
				boost::beast::error_code ec,
				std::size_t bytes_transferred)
		{
			boost::ignore_unused(bytes_transferred);

			if (ec)
			{
				LOG_ERR("http write: %s", ec.message().c_str());
				if (res_callback_ != nullptr) {
					res_callback_("", false);
				}
				return;
			}

			// Receive the HTTP response
			boost::beast::http::async_read(stream_, buffer_, res_,
				boost::beast::bind_front_handler(
					&HttpClientSession::on_read,
					shared_from_this()));
		}

		void
			HttpClientSession::on_read(
				boost::beast::error_code ec,
				std::size_t bytes_transferred)
		{
			boost::ignore_unused(bytes_transferred);

			if (ec)
			{
				LOG_ERR("http read: %s", ec.message().c_str());
				if (res_callback_ != nullptr) {
					res_callback_("", false);
				}
				return;
			}

			// Write the message to standard out
			if (res_callback_ != nullptr){
				res_callback_(res_.body().data(), true);
			}
			//std::cout << res_.body().data() << std::endl;

			// Gracefully close the socket
			stream_.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);

			// not_connected happens sometimes so don't bother reporting it.
			if (ec && ec != boost::system::errc::not_connected)
			{
				LOG_ERR("http conn shutdown: %s", ec.message().c_str());
				return;
			}

			// If we get here then the connection is closed gracefully
		}
	}
}