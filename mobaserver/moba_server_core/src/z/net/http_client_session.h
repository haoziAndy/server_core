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


namespace z {
	namespace net {
		// Performs an HTTP POST and prints the response
		class HttpClientSession : public std::enable_shared_from_this<HttpClientSession>
		{
			boost::asio::ip::tcp::resolver resolver_;
			boost::asio::ip::tcp::socket socket_;
			boost::beast::flat_buffer buffer_; // (Must persist between reads)
			boost::beast::http::request<boost::beast::http::string_body> req_;
			boost::beast::http::response<boost::beast::http::string_body> res_;
			std::function<void(const std::string&, bool)> res_callback_;

		public:
			// Resolver and socket require an io_context
			explicit
				HttpClientSession(boost::asio::io_context& ioc)
				: resolver_(ioc)
				, socket_(ioc)
			{
			}

			// Start the asynchronous operation
			void
				run(
					char const* host,
					char const* port,
					char const* target,
					const std::string &body,
					const int version,
					std::function<void(const std::string&, bool)> callback);

			void
				on_resolve(
					boost::system::error_code ec,
					boost::asio::ip::tcp::resolver::results_type results);


			void
				on_connect(boost::system::error_code ec);

			void
				on_write(
					boost::system::error_code ec,
					std::size_t bytes_transferred);

			void
				on_read(
					boost::system::error_code ec,
					std::size_t bytes_transferred);
		};
	}
}