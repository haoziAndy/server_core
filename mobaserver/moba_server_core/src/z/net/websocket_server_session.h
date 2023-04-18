#pragma once

namespace z {
	namespace net {

		// Handles an HTTP server connection
		class WSServerSession : public std::enable_shared_from_this<WSServerSession>
		{
		public:
			uint32 session_id_;

		private:
			boost::beast::websocket::stream<boost::beast::tcp_stream> ws_;
			boost::beast::flat_buffer buffer_;

		private:
			IHttpRequestHandler* msg_handler_;

			int session_id_;
			std::string user_id_;

			char read_data_[recv_buff_length];
			int32 read_size_;

			std::atomic_uint op_count_;

			bool is_reading_;
			bool is_writing_;
			bool is_closing_;


			std::vector<boost::asio::const_buffer> send_queue_;
			std::vector<boost::asio::const_buffer> pending_send_queue_;

		public:
			explicit
				// Take ownership of the socket
				WSServerSession(
					boost::asio::ip::tcp::socket&& socket,
					IHttpRequestHandler* msg_handler, const uint32 session_id);
			// Start the asynchronous operation
			void run();

			void on_run();

			void  on_accept(boost::beast::error_code ec);

			void do_read();

			void on_read(boost::beast::error_code ec,std::size_t bytes_transferred);

			void
				on_write(
					bool close,
					boost::beast::error_code ec,
					std::size_t bytes_transferred);

			void response(const std::string &content);


			void
				do_close();


			DISALLOW_COPY_AND_ASSIGN(WSServerSession);

		};
	}
}