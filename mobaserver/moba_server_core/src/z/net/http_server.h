#pragma once

namespace z {
	namespace net {
		
		class HServer
		{

		public:
			HServer();
			// Start accepting incoming connections
			bool Init(boost::asio::ip::tcp::endpoint endpoint, IHttpRequestHandler* msg_handler);
			void run();
			void do_accept();
			void on_accept(boost::system::error_code ec);
			void stop();
			IHttpRequestHandler* msg_handler() const { return msg_handler_; }
			
		private:

			boost::asio::ip::tcp::acceptor acceptor_;
			boost::asio::ip::tcp::socket socket_;
			IHttpRequestHandler* msg_handler_;

			DECLARE_SINGLETON(HServer);
		};
	}
}

#define HSERVER Singleton<z::net::HServer>::instance()