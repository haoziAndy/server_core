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


namespace z {
	namespace net {
		// Start the asynchronous operation
		void HttpClientSession::run(
				char const* host,
				char const* port,
				char const* target,
				const std::string &body,
				const int version,
			    std::function<void(const std::string&, bool)> callback)
		{

			// Set up an HTTP GET request message
			req_.version(version);
			req_.method(boost::beast::http::verb::post);
			req_.target(target);
			req_.set(boost::beast::http::field::host, host);
			req_.set(boost::beast::http::field::user_agent, BOOST_BEAST_VERSION_STRING);
			req_.set(boost::beast::http::field::content_type, "application/x-www-form-urlencoded; charset=utf-8");
			req_.body() = body;
			req_.prepare_payload();
			res_callback_ = callback;
			// Look up the domain name
			resolver_.async_resolve(
				host,
				port,
				std::bind(
					&HttpClientSession::on_resolve,
					shared_from_this(),
					std::placeholders::_1,
					std::placeholders::_2));
		}

		void
			HttpClientSession::on_resolve(
				boost::system::error_code ec,
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

			// Make the connection on the IP address we get from a lookup
			boost::asio::async_connect(
				socket_,
				results.begin(),
				results.end(),
				std::bind(
					&HttpClientSession::on_connect,
					shared_from_this(),
					std::placeholders::_1));
		}

		void
			HttpClientSession::on_connect(boost::system::error_code ec)
		{
			if (ec)
			{
				LOG_ERR("http connect: %s", ec.message().c_str());
				if (res_callback_ != nullptr) {
					res_callback_("", false);
				}
				return;
			}

			// Send the HTTP request to the remote host
			boost::beast::http::async_write(socket_, req_,
				std::bind(
					&HttpClientSession::on_write,
					shared_from_this(),
					std::placeholders::_1,
					std::placeholders::_2));
		}

		void
			HttpClientSession::on_write(
				boost::system::error_code ec,
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
			boost::beast::http::async_read(socket_, buffer_, res_,
				std::bind(
					&HttpClientSession::on_read,
					shared_from_this(),
					std::placeholders::_1,
					std::placeholders::_2));
		}

		void
			HttpClientSession::on_read(
				boost::system::error_code ec,
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
			socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);

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