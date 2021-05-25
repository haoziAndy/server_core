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
			void on_accept(boost::beast::error_code ec, boost::asio::ip::tcp::socket socket);
			void stop();
			IHttpRequestHandler* msg_handler() const { return msg_handler_; }
			void remove_session(const uint32 http_session_id);
			void response(const uint32 http_session_id, const std::string& content);
		private:
			void CheckHandleReqTimeout(const boost::system::error_code& ec);
		private:

			boost::asio::io_context& ioc_;
			boost::asio::ip::tcp::acceptor acceptor_;
			IHttpRequestHandler* msg_handler_;
			std::unordered_map<uint32, std::shared_ptr<HServerSession>> connection_mgr_;
			boost::asio::deadline_timer deadline_timer_;

			DECLARE_SINGLETON(HServer);
		};
	}
}

#define HSERVER Singleton<z::net::HServer>::instance()