#pragma once

namespace z {
	namespace net {
		
		class HServer : public std::enable_shared_from_this<HServer>
		{

		public:
			HServer();
			// Start accepting incoming connections
			bool Init(boost::asio::ip::tcp::endpoint endpoint, IHttpRequestHandler* msg_handler);
			void run();
			void do_accept();
			void on_accept(boost::beast::error_code ec, boost::asio::ip::tcp::socket socket);
			void stop();
			IHttpRequestHandler* msg_handler() const { return msg_handler_; }
			
		private:

			boost::asio::io_context& ioc_;
			boost::asio::ip::tcp::acceptor acceptor_;
			IHttpRequestHandler* msg_handler_;

			DECLARE_SINGLETON(HServer);
		};
	}
}

#define HSERVER Singleton<z::net::HServer>::instance()