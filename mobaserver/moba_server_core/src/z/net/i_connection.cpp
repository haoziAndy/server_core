#include "stdafx.h"
#include "i_connection.h"
#include "i_server.h"
#include "msg_header.h"

namespace z {
namespace net {

IConnection::IConnection( IServer* server, int conn_index )
    : server_(server)
    , socket_(server->io_service())
    , deadline_timer_(server->io_service())
    , session_id_(conn_index)
    , user_id_(0)
    , read_size_(0)
    , is_reading_(false)
    , is_writing_(false)
    , is_closing_(false)
    , pending_send_queue_(send_queue_)
{
    op_count_ = 0;
}

IConnection::~IConnection()
{
    boost::system::error_code ignored_ec;
    deadline_timer_.cancel(ignored_ec);
}

void IConnection::Start()
{
    DoStart();
}

void IConnection::DoStart()
{
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
            LOG_INFO("pending_send_queue_ too large %d", pending_send_queue_.size());
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
            LOG_INFO("pending_send_queue_ too large %d", pending_send_queue_.size());
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
            LOG_INFO("pending_send_queue_ too large %d", pending_send_queue_.size());
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
    socket_.async_read_some(boost::asio::buffer(read_data_ + read_size_, sizeof(read_data_) - read_size_),
        boost::bind(&IConnection::HandleRead, this,
        boost::asio::placeholders::error,
        boost::asio::placeholders::bytes_transferred));
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
    boost::asio::async_write(socket_,
        send_queue_,
        boost::bind(&IConnection::HandleWrite, this, 
        boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
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
        socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored_ec);
        socket_.close(ignored_ec);
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

