#pragma once

namespace z {
	namespace net {
		template<
			class Body, class Allocator,
			class Send>
			void handle_request(IHttpRequestHandler* msg_handler, boost::beast::http::request<Body, boost::beast::http::basic_fields<Allocator>>&& req,
				Send&& send, const uint32 http_session_id)
		{
			// Returns a bad request response
			auto const bad_request =
				[&req](boost::beast::string_view why)
			{
				boost::beast::http::response<boost::beast::http::string_body> res{ boost::beast::http::status::bad_request, req.version() };
				res.set(boost::beast::http::field::server, BOOST_BEAST_VERSION_STRING);
				res.set(boost::beast::http::field::content_type, "application/json");
				res.keep_alive(req.keep_alive());
				res.body() = std::string(why);
				res.prepare_payload();
				return res;
			};

			// Returns a not found response
			/*auto const not_found =
				[&req](boost::beast::string_view target)
			{
				boost::beast::http::response<boost::beast::http::string_body> res{ boost::beast::http::status::not_found, req.version() };
				res.set(boost::beast::http::field::server, BOOST_BEAST_VERSION_STRING);
				res.set(boost::beast::http::field::content_type, "application/json");
				res.keep_alive(req.keep_alive());
				res.body() = "The resource '" + std::string(target) + "' was not found.";
				res.prepare_payload();
				return res;
			};*/

			// Returns a server error response
			auto const server_error =
				[&req](boost::beast::string_view what)
			{
				boost::beast::http::response<boost::beast::http::string_body> res{ boost::beast::http::status::internal_server_error, req.version() };
				res.set(boost::beast::http::field::server, BOOST_BEAST_VERSION_STRING);
				res.set(boost::beast::http::field::content_type, "application/json");
				res.keep_alive(req.keep_alive());
				res.body() = "An error occurred: '" + std::string(what) + "'";
				res.prepare_payload();
				return res;
			};

			// Make sure we can handle the method
			if (req.method() != boost::beast::http::verb::post &&
				req.method() != boost::beast::http::verb::head)
			{
				return send(bad_request("Unknown HTTP-method"));
			}

			// Request path must be absolute and not contain "..".
			if (req.target().empty() ||
				req.target()[0] != '/' ||
				req.target().find("..") != boost::beast::string_view::npos)
			{
				return send(bad_request("Illegal request-target"));
			}

			if (msg_handler == nullptr) {
				return send(server_error("msg_handler == nullptr"));
			}

			msg_handler->OnRequest(req.body(), http_session_id);
			//const std::string result = msg_handler->OnRequest(req.body());
			//boost::beast::http::string_body::value_type body(result);

			// Cache the size since we need it after the move
			//auto const size = body.size();
			// Respond to HEAD request
			/*if (req.method() == boost::beast::http::verb::head)
			{
				boost::beast::http::response<boost::beast::http::empty_body> res{ boost::beast::http::status::ok, req.version() };
				res.set(boost::beast::http::field::server, BOOST_BEAST_VERSION_STRING);
				res.set(boost::beast::http::field::content_type, "application/json");
				res.content_length(size);
				res.keep_alive(req.keep_alive());
				return send(std::move(res));
			}*/

			// Respond to GET request
			/*boost::beast::http::response<boost::beast::http::string_body> res{
				std::piecewise_construct,
				std::make_tuple(std::move(body)),
				std::make_tuple(boost::beast::http::status::ok, req.version()) };
			res.set(boost::beast::http::field::server, BOOST_BEAST_VERSION_STRING);
			res.set(boost::beast::http::field::content_type, "application/json");
			res.content_length(size);
			res.keep_alive(req.keep_alive());
			return send(std::move(res));*/

		}

		// Handles an HTTP server connection
		class HServerSession : public std::enable_shared_from_this<HServerSession>
		{
			// This is the C++11 equivalent of a generic lambda.
			// The function object is used to send an HTTP message.
			struct send_lambda
			{
				HServerSession& self_;

				explicit
					send_lambda(HServerSession& self)
					: self_(self)
				{
				}

				template<bool isRequest, class Body, class Fields>
				void
					operator()(boost::beast::http::message<isRequest, Body, Fields>&& msg) const
				{
					// The lifetime of the message has to extend
					// for the duration of the async operation so
					// we use a shared_ptr to manage it.
					auto sp = std::make_shared<
						boost::beast::http::message<isRequest, Body, Fields>>(std::move(msg));

					// Store a type-erased version of the shared
					// pointer in the class to keep it alive.
					self_.res_ = sp;

					// Write the response
					boost::beast::http::async_write(
						self_.stream_,
						*sp,
						boost::beast::bind_front_handler(
							&HServerSession::on_write,
							self_.shared_from_this(),
							sp->need_eof()));
				}
			};

			boost::beast::tcp_stream stream_;
			boost::beast::flat_buffer buffer_;
			boost::beast::http::request<boost::beast::http::string_body> req_;
			std::shared_ptr<void> res_;
			send_lambda send_lambda_;
			uint32 session_id_;

		private:
			bool request_keep_alive_;
			unsigned request_version_;
			uint32 start_handle_req_ts_;


		private:
			IHttpRequestHandler* msg_handler_;

		public:
			explicit
				// Take ownership of the socket
				HServerSession(
					boost::asio::ip::tcp::socket&& socket,
					IHttpRequestHandler* msg_handler, const uint32 session_id);
			// Start the asynchronous operation
			void
				run();

			void
				do_read();

			void
				on_read(
					boost::beast::error_code ec,
					std::size_t bytes_transferred);

			void
				on_write(
					bool close,
					boost::beast::error_code ec,
					std::size_t bytes_transferred);

			void response(const std::string &content);


			void
				do_close();

			bool handle_req_timeout();

		};
	}
}