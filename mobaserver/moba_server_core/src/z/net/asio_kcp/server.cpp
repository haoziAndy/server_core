#include "stdafx.h"
#include "server.hpp"
#include <boost/bind.hpp>
#include <signal.h>
#include <cstdlib>
#include "connection_manager.hpp"


namespace kcp_svr {

server::server(boost::asio::io_service& io_service)
  : io_service_(io_service),
    connection_manager_ptr_(new connection_manager(io_service_))
{
}

void server::stop()
{
    // The server is stopped by cancelling all outstanding asynchronous
    // operations. Once all operations have finished the io_service::run() call
    // will exit.
    connection_manager_ptr_->stop_all();


    // todo: when running UdpPacketHandler in work thread pool
    // change to this:
    //     connection_manager_.stop_recv();
    //     connection_manager_.stop_packet_handler_workthread();
    //     connection_manager_.close();
}

bool server::init(const std::string& address, const std::string& port)
{
	return connection_manager_ptr_->connection_manager_init(address, std::atoi(port.c_str()));
}

void server::set_callback(const std::function<event_callback_t>& func)
{
    connection_manager_ptr_->set_callback(func);
}

void server::force_disconnect(const kcp_conv_t& conv)
{
    connection_manager_ptr_->force_disconnect(conv);
}


int server::send_msg(const kcp_conv_t& conv, std::shared_ptr<std::string> msg)
{
    return connection_manager_ptr_->send_msg(conv, msg);
}

int server::send_msg(const kcp_conv_t& conv, char *msg,const int32 length)
{
	return connection_manager_ptr_->send_msg(conv, msg,length);
}


} // namespace kcp_svr
