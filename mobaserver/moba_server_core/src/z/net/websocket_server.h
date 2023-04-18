#pragma once

namespace z {
	namespace net {
		
		class WSServer
		{

		public:
			WSServer();
			// Start accepting incoming connections
			bool Init(boost::asio::ip::tcp::endpoint endpoint, IHttpRequestHandler* msg_handler);
			void run();
			void do_accept();
			void on_accept(boost::beast::error_code ec, boost::asio::ip::tcp::socket socket);

			boost::asio::io_context& ioc_;
			boost::asio::ip::tcp::acceptor acceptor_;
			IHttpRequestHandler* msg_handler_;
			std::unordered_map<uint32, std::shared_ptr<WSServerSession>> connection_mgr_;
			boost::asio::deadline_timer deadline_timer_;

		private:

			DECLARE_SINGLETON(WSServer);
		};
	}
}

#define WSSERVER Singleton<z::net::WSServer>::instance()