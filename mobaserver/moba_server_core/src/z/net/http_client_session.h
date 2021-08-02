#pragma once


namespace z {
	namespace net {
		// Performs an HTTP POST and prints the response
		class HttpClientSession : public std::enable_shared_from_this<HttpClientSession>
		{
			boost::asio::ip::tcp::resolver resolver_;
			boost::beast::tcp_stream stream_;
			boost::beast::flat_buffer buffer_; // (Must persist between reads)
			boost::beast::http::request<boost::beast::http::string_body> req_;
			boost::beast::http::response<boost::beast::http::string_body> res_;
			std::function<void(const std::string&, bool)> res_callback_;

		public:
			// Resolver and socket require an io_context
			explicit
				HttpClientSession(boost::asio::io_context& ioc)
				: resolver_(boost::asio::make_strand(ioc))
				, stream_(boost::asio::make_strand(ioc))
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
					boost::beast::error_code ec,
					boost::asio::ip::tcp::resolver::results_type results);


			void
				on_connect(boost::beast::error_code ec, boost::asio::ip::tcp::resolver::results_type::endpoint_type);

			void
				on_write(
					boost::beast::error_code ec,
					std::size_t bytes_transferred);

			void
				on_read(
					boost::beast::error_code ec,
					std::size_t bytes_transferred);

		};
	}
}