#include "stdafx.h"
#include "connection_container.hpp"
#include <algorithm>

#include "ikcp.h"
#include <cstdlib>
#include <iostream>


namespace kcp_svr {

connection_container::connection_container(void)
{
}

connection::shared_ptr connection_container::find_by_conv(const kcp_conv_t& conv)
{
    auto iter = connections_.find(conv);
    if (iter == connections_.end())
        return connection::shared_ptr();
    else
        return iter->second;
}

void connection_container::update_all_kcp(uint32_t clock)
{
	//			
	std::unordered_map<kcp_conv_t, connection::shared_ptr>  timeout_list;
    for (auto iter = connections_.begin(); iter != connections_.end();++iter)
    {
        connection::shared_ptr& ptr = iter->second;

        ptr->update_kcp(clock);
		
        // check timeout
        if (ptr->is_timeout())
        {
           // ptr->do_timeout();
			timeout_list[iter->first] = iter->second;
        }
    }

	for (auto iter = timeout_list.begin(); iter != timeout_list.end(); ++iter)
	{
		UDPSERVER.CloseConnection(iter->second->session_id());
	}

}

void connection_container::stop_all()
{
    // todo need more code if connection bind some asio callback.

    connections_.clear();
}

connection::shared_ptr connection_container::add_new_connection(std::weak_ptr<connection_manager> manager_ptr,
        const kcp_conv_t& conv, const udp::endpoint& udp_sender_endpoint)
{
    connection::shared_ptr ptr = connection::create(manager_ptr, conv, udp_sender_endpoint);
    connections_[conv] = ptr;

	const int32 session_id = UDPSERVER.AddNewConnection(conv);
	ptr->set_session_id(session_id);
	return ptr;
}

void connection_container::remove_connection(const kcp_conv_t& conv)
{
    connections_.erase(conv);
}

kcp_conv_t connection_container::get_new_conv(void) const
{
    // todo using rand to get a conv. privent the attack from guess conv.
    // and must bigger than 1000

    // increase from 1001, must bigger than 1000
    static uint32_t static_cur_conv = 1000;
    static_cur_conv++;
    return static_cur_conv;
}

} // namespace kcp_svr
