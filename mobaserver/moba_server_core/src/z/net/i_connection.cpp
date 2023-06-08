#include "stdafx.h"
#include "i_connection.h"
#include "i_server.h"
#include "msg_header.h"

namespace z {
namespace net {

#ifndef USE_WEBSOCKET
	IConnection::IConnection(IServer* server, int conn_index)
		: server_(server)
		, socket_(server->io_service())
		, deadline_timer_(server->io_service())
		, session_id_(conn_index)
		, user_id_("")
		, read_size_(0)
		, is_reading_(false)
		, is_writing_(false)
		, is_closing_(false)
		, pending_send_queue_(send_queue_)
	{
		op_count_ = 0;
	}
#endif // !USE_WEBSOCKET

#ifdef USE_WEBSOCKET

	IConnection::IConnection(IServer* server, int conn_index, boost::asio::ip::tcp::socket &&socket)
		: server_(server)
#ifdef USE_WEBSOCKET_WITH_SSL
		, web_socket_(std::move(socket),server->GetSSlCtx())
#else
		, web_socket_(std::move(socket))
#endif // USE_WEBSOCKET_WITH_SSL
		, deadline_timer_(server->io_service())
		, session_id_(conn_index)
		, user_id_("")
		, read_size_(0)
		, is_reading_(false)
		, is_writing_(false)
		, is_closing_(false)
		, pending_send_queue_(send_queue_)
	{
		op_count_ = 0;
	}

#endif // USE_WEBSOCKET

IConnection::~IConnection()
{
    boost::system::error_code ignored_ec;
    deadline_timer_.cancel(ignored_ec);
}

#ifdef USE_WEBSOCKET
void IConnection::WebRun()
{
	OnRun();
	//boost::beast::net::dispatch(web_socket_.get_executor(),
	//	boost::beast::bind_front_handler(
	//		&IConnection::OnRun,
	//		this));
}

void IConnection::OnRun()
{
	++op_count_;
#ifdef USE_WEBSOCKET_WITH_SSL
	// Set the timeout.
	boost::beast::get_lowest_layer(web_socket_).expires_after(std::chrono::seconds(30));

	// Perform the SSL handshake
	web_socket_.next_layer().async_handshake(
		boost::asio::ssl::stream_base::server,
		boost::beast::bind_front_handler(
			&IConnection::OnWebHandShake,
			this));
#else
	web_socket_.set_option(
		boost::beast::websocket::stream_base::timeout::suggested(
			boost::beast::role_type::server));

	// Set a decorator to change the Server of the handshake
	web_socket_.set_option(boost::beast::websocket::stream_base::decorator(
		[](boost::beast::websocket::response_type& res)
	{
		res.set(boost::beast::http::field::server,
			std::string(BOOST_BEAST_VERSION_STRING) +
			" websocket-server-async");
	}));

	// Accept the websocket handshake
	web_socket_.async_accept(
		boost::beast::bind_front_handler(
			&IConnection::OnWebAccept,
			this));
#endif // USE_WEBSOCKET_WITH_SSL

}

void IConnection::OnWebHandShake(boost::beast::error_code ec)
{
	--op_count_;
	if (ec)
	{
		LOG_DEBUG("Handshake,err %s", ec.message().c_str());
		AsyncClose();
		return;
	}
	if (is_closing_)
		return;

	++op_count_;
	boost::beast::get_lowest_layer(web_socket_).expires_never();

	web_socket_.set_option(
		boost::beast::websocket::stream_base::timeout::suggested(
			boost::beast::role_type::server));

	// Set a decorator to change the Server of the handshake
	web_socket_.set_option(boost::beast::websocket::stream_base::decorator(
		[](boost::beast::websocket::response_type& res)
	{
		res.set(boost::beast::http::field::server,
			std::string(BOOST_BEAST_VERSION_STRING) +
			" websocket-server-async-ssl");
	}));

	// Accept the websocket handshake
	web_socket_.async_accept(
		boost::beast::bind_front_handler(
			&IConnection::OnWebAccept,
			this));

}

void IConnection::OnWebAccept(boost::beast::error_code ec)
{
	--op_count_;
	if (ec)
	{
		AsyncClose();
		return;
	}
	Start();
}
#endif

void IConnection::Start()
{
    DoStart();
}

void IConnection::DoStart()
{
#ifndef USE_WEBSOCKET
    boost::system::error_code set_option_err;
    boost::asio::ip::tcp::no_delay no_delay(true);
    socket_.set_option(no_delay, set_option_err);
    if (set_option_err)
    {
        LOG_ERR("ICConnection::DoStart() failed, set_option_err[%d]:%s",
            set_option_err.value(), set_option_err.message().c_str());
        AsyncClose();
        return;
    }
#endif

    StartRead();
    return;
}

void IConnection::AsyncSend(const char* data, int32 length)
{
    if (!is_writing_)
    {
        send_queue_.push_back(boost::asio::buffer(data, length));
        StartWrite();
    }
    else
    {
        enum {max_pending_send_size = 256};
        // 增加保护, 防止不停的累加
        if (pending_send_queue_.size() >= max_pending_send_size)
        {
            LOG_INFO("pending_send_queue_ too large %d", static_cast<int>(pending_send_queue_.size()));
            AsyncClose();
            return;
        }
        pending_send_queue_.push_back(boost::asio::buffer(data, length));
    }
}

void IConnection::AsyncSend( const boost::asio::const_buffer& buffer )
{
    if (!is_writing_)
    {
        send_queue_.push_back(buffer);
        StartWrite();
    }
    else
    {
        enum {max_pending_send_size = 1024};
        // 增加保护, 防止不停的累加
        if (pending_send_queue_.size() >= max_pending_send_size)
        {
            LOG_INFO("pending_send_queue_ too large %d", static_cast<int>( pending_send_queue_.size()));
            AsyncClose();
            return;
        }
        pending_send_queue_.push_back(buffer);
    }
}

void IConnection::AsyncSend( const std::vector<boost::asio::const_buffer>& buffers )
{
    if (!is_writing_)
    {
        send_queue_.insert(send_queue_.end(), buffers.begin(), buffers.end());
        StartWrite();
    }
    else
    {
        enum {max_pending_send_size = 256};
        // 增加保护, 防止不停的累加
        if (pending_send_queue_.size() >= max_pending_send_size)
        {
            LOG_INFO("pending_send_queue_ too large %d", static_cast<int>(pending_send_queue_.size()));
            AsyncClose();
            return;
        }
        pending_send_queue_.insert(pending_send_queue_.end(), buffers.begin(), buffers.end());
    }
}

void IConnection::StartRead()
{
    if (is_closing_)
        return;
    if (is_reading_)
        return;
    is_reading_ = true;

    ++op_count_;

#ifdef USE_WEBSOCKET
	web_socket_.async_read_some(boost::asio::buffer(read_data_ + read_size_, sizeof(read_data_) - read_size_),
		boost::bind(&IConnection::HandleRead, this,
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred));
#else
    socket_.async_read_some(boost::asio::buffer(read_data_ + read_size_, sizeof(read_data_) - read_size_),
        boost::bind(&IConnection::HandleRead, this,
        boost::asio::placeholders::error,
        boost::asio::placeholders::bytes_transferred));
#endif

}

void IConnection::HandleRead( const boost::system::error_code& ec, size_t bytes_transferred )
{
    -- op_count_;
    is_reading_ = false;
    if (!ec)
    {
        /// @todo decryt

        read_size_ += bytes_transferred;
        int32 handled_size = 0;
        while (handled_size < read_size_)
        {
            int len = OnRead(read_data_ + handled_size, read_size_ - handled_size);
            if (len == 0)
            {
                break;
            }
            else if (len > 0)
            {
                handled_size += len;
            }
            else
            {
                AsyncClose();
                return;
            }
        }

        if (handled_size > 0 && handled_size < read_size_)
        {
            memcpy(read_data_, 
                read_data_ + handled_size, read_size_ - handled_size);
        }
        read_size_ -= handled_size;

        // continue read
        StartRead();
    }
    else
    {
        HandleError(ec);
    }

    if (op_count_ == 0)
    {
        AsyncClose();
    }
}

void IConnection::StartWrite()
{
    if (is_closing_)
        return;

    if (is_writing_)
        return;

    if (send_queue_.empty())
        return;

    is_writing_ = true;

    ++op_count_;

#ifdef USE_WEBSOCKET
	web_socket_.binary(web_socket_.got_binary());
	web_socket_.async_write(send_queue_,
		boost::bind(&IConnection::HandleWrite, this,
			boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
#else
	//使用tcp
	boost::asio::async_write(socket_,
		send_queue_,
		boost::bind(&IConnection::HandleWrite, this,
			boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));

#endif
}

void IConnection::HandleWrite( const boost::system::error_code& ec, size_t bytes_transferred )
{
    -- op_count_;
    is_writing_ = false;
    if (!ec)
    {
        OnWrite(bytes_transferred);
        send_queue_.clear();
        if (!pending_send_queue_.empty())
        {
            send_queue_.swap(pending_send_queue_);
            // continue write
            StartWrite();
        }
    }
    else
    {
        HandleError(ec);
    }

    if (op_count_ == 0)
    {
        AsyncClose();
    }
}

void IConnection::Close()
{
    if (is_closing_)
    {
        if (op_count_ == 0)
            Destroy();
    }
    else
    {
        is_closing_ = true;
        
		boost::system::error_code ignored_ec;
#ifdef USE_WEBSOCKET
		web_socket_.close(boost::beast::websocket::close_code::normal,ignored_ec);
#else
        socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored_ec);
        socket_.close(ignored_ec);
#endif
        deadline_timer_.cancel(ignored_ec);

        OnClosed();
    }
}

void IConnection::HandleError( const boost::system::error_code& ec )
{
    if (is_closing_)
        return;

    if (ec == boost::asio::error::operation_aborted)
        return;

    if (!(ec == boost::asio::error::connection_reset ||
        ec == boost::asio::error::eof))
    {
        LOG_DEBUG("session[%d] error[%d]:%s", session_id(), ec.value(), ec.message().c_str());
    }
    AsyncClose();
}

void IConnection::Destroy()
{
	LOG_DEBUG("IConnection::Destroy");
    ZPOOL_DELETE(this);
}

void IConnection::AsyncClose()
{
    if (!is_closing_)
    {
        server_->CloseConnection(session_id());
    }
    else
    {
        Close();
    }
}

boost::asio::io_service& IConnection::io_service() const
{
    return server_->io_service();
}





} // namespace net
} // namespace z

