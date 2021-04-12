#include "stdafx.h"
#include "z_server.h"
#include "msg_header.h"
#include "z_receiver.h"
#include "z_sender.h"
#include "http_client_session.h"

namespace z {
namespace net {


//int ZServer::Init(const char* server_config_file, int server_group_id, int server_type, ISMsgHandler* msg_handler)
int ZServer::Init(int server_id, int server_type, const std::string& server_name, 
                  const std::string& listen_str, const std::string& conn_str, ISMsgHandler* msg_handler)
{
    server_id_ = server_id;
    server_type_ = server_type;
    server_name_ = server_name;
    server_conn_str_ = conn_str;
    msg_handler_ = msg_handler;

	int major, minor, patch;
	zmq_version(&major, &minor, &patch); 
	LOG_INFO("Current ZEROMQ version is %d.%d.%d", major, minor, patch);

    if (AddReceiver(server_id, listen_str, msg_handler) == NULL)
    {
        LOG_ERR("AddReceiver failed server_id %d, listern_str %s", server_id, listen_str.data());
        return -1;
    }

    signals_.add(SIGINT);
    signals_.add(SIGTERM);
#if defined(SIGQUIT)
    signals_.add(SIGQUIT);
#endif // defined(SIGQUIT)
    signals_.async_wait(boost::bind(&ZServer::Stop, this));

	this->set_server_status(ZServerStatus::ZServerStatus_INIT);

    return 0;
}

int ZServer::Destroy()
{
    for (auto it=clean_functions_.rbegin(); it!=clean_functions_.rend(); ++it)
    {
        auto& functor = *it;
        functor();
    }

    for (Receivers::iterator it=receivers_.begin();
            it!=receivers_.end(); ++it)
    {
        if (it->second != nullptr)
        {
            delete it->second;
            it->second = nullptr;
        }
    }
    receivers_.clear();
    

    for (Senders::iterator it=senders_.begin();
            it!=senders_.end(); ++it)
    {
        if (it->second != nullptr)
        {
            delete it->second;
			it->second = nullptr;
        }
    }
    senders_.clear();
    type_senders_.clear();
    asio_ref_zsockets_.clear();
    return 0;
}

ZReceiver* ZServer::AddReceiver(const int server_id, const std::string& listen_str, ISMsgHandler* msg_handler)
{
    int ret;
    ZReceiver* receiver = new ZReceiver();
    ret = receiver->Init(server_id, ZMQ_PULL, listen_str.c_str());
    if (ret != 0)
    {
        delete receiver;
        receiver = nullptr;
        return nullptr;
    }

    receiver->SetSMsgHandler(msg_handler);

    if (!receivers_.insert(std::make_pair(server_id, receiver)).second)
    {
        delete receiver;
        receiver = nullptr;
        return nullptr;
    }
    
    ret = AddPollItem(receiver);

    if (is_delegate_looping_)
    {
        int fd = -1;
        std::size_t fd_size = sizeof(fd);
        if (zmq_getsockopt(receiver->socket(), ZMQ_FD, &fd, &fd_size) < 0)
        {
            LOG_ERR("failed zmq_getsockopt() %d: %s", zmq_errno(), zmq_strerror(zmq_errno()));
            receivers_.erase(server_id);
            delete receiver;
            receiver = nullptr;
            return nullptr;
        }

        boost::shared_ptr<boost::asio::ip::tcp::socket> socket(new boost::asio::ip::tcp::socket(time_engine_));
        socket->assign(boost::asio::ip::tcp::v4(), fd);
        auto ret = asio_ref_zsockets_.insert(std::make_pair(receiver->server_id(), socket));
        if (ret.second)
            CheckReceiverReadable(server_id);
    }
    
    return receiver;
}

ZSender* ZServer::AddSender( const int sendto_server_id, const std::string& connect_str, int server_type, const std::string& server_name, bool need_verify )
{
    ZSender* sender = GetSender(sendto_server_id);
    if (sender != nullptr)
        return sender;

    int ret;
    sender = new ZSender();
    ret = sender->Init(sendto_server_id, ZMQ_PUSH, connect_str, server_name);
    if (ret != 0)
    {
        delete sender;
        sender = nullptr;
        return nullptr;
    }
    if (sendto_server_id <= 0)
    {
        delete sender;
        sender = nullptr;
        return nullptr;
    }

	if (senders_[sendto_server_id] != nullptr) {
		LOG_ERR("Sender = %d already own", sendto_server_id);
	}
    senders_[sendto_server_id] = sender;

    if (server_type != 0)
	{
		type_senders_[server_type] = sender;
    }
    if (need_verify)
        check_sender_flags_.push_back(std::make_pair(sendto_server_id, 0));

    return sender;
}

const std::vector<ZSender*> ZServer::GetSendersByType(uint32 server_type) const
{
    std::vector<ZSender*> result;
    if (senders_.empty())
    {
        return result;
    }
	for (auto it = senders_.begin(); it != senders_.end(); ++it)
	{
		if (SERVER_TYPE(it->first) == server_type)
		{
			if (it->second == nullptr)
			{
				continue;
			}
			result.push_back(it->second);
		}
	}

    return result;
}

int ZServer::AddPollItem( ZReceiver* receiver )
{
    zmq_pollitem_t item;
    item.socket = receiver->socket();
    item.fd = 0;
    item.events = ZMQ_POLLIN;
    item.revents = 0;

    poll_items_.push_back(item);
    ref_poll_receivers_.push_back(receiver);

    return 0;
}

void ZServer::Run()
{
    // 开始同其他服务器握手
    StartHandShakeServers();
    // 默认asio loop
    DelegateLoop();
    time_engine_.run();

    return;
}

void ZServer::DelegateLoop()
{
    if (is_delegate_looping_)
    {
        LOG_ERR("is_delegate_looping_ already DelegateLoop");
        return;
    }
    is_delegate_looping_ = true;
    if (!asio_ref_zsockets_.empty())
    {
        LOG_ERR("asio_ref_zsockets_ is not empty on DelegateLoop");
        asio_ref_zsockets_.clear();
    }

    for (auto it = receivers_.begin(); it != receivers_.end(); ++it)
    {
        ZReceiver* receiver = it->second;
        if (!receiver)
            continue;
        int fd = -1;
        std::size_t fd_size = sizeof(fd);
        if (zmq_getsockopt(receiver->socket(), ZMQ_FD, &fd, &fd_size) < 0)
        {
            LOG_ERR("failed zmq_getsockopt() %d: %s", zmq_errno(), zmq_strerror(zmq_errno()));
            continue;
        }

        boost::shared_ptr<boost::asio::ip::tcp::socket> socket(new boost::asio::ip::tcp::socket(time_engine_));
        socket->assign(boost::asio::ip::tcp::v4(), fd);
        asio_ref_zsockets_.insert(std::make_pair(receiver->server_id(), socket));
    }
    for (auto it = asio_ref_zsockets_.begin(); it != asio_ref_zsockets_.end(); ++it)
    {
        CheckReceiverReadable(it->first);
    }
}

void ZServer::Stop()
{
    if (!is_server_shutdown_)
    {
        LOG_INFO( "Server was stop" );
        is_server_shutdown_ = true;

        for (auto it = senders_.begin(); it != senders_.end(); ++it)
        {
            ZSender* sender = it->second;
            if (sender)
                sender->Close();
        }
		senders_.clear();
        for (auto it=receivers_.begin(); it!=receivers_.end(); ++it)
        {
            auto receiver = it->second;
            if (receiver)
                receiver->Destroy();
        }
		receivers_.clear();
		time_engine_.stop();
		this->set_server_status(ZServerStatus::ZServerStatus_STOP);
    }

    return;
}

void ZServer::ForceStop()
{
    if (!is_server_shutdown_)
    {
        for (auto it = senders_.begin(); it != senders_.end(); ++it)
        {
            ZSender* sender = it->second;
            if (!sender)
                continue;
            sender->Close(true);
        }
        senders_.clear();
    }

    Stop();
}

int ZServer::SendMsg( const int sendto_server_id, SMsgHeader* header, const google::protobuf::Message* msg )
{
    if (sendto_server_id < 0 )
    {
        LOG_ERR("Wrong sendto_server_id %d. OUT OF index %d.", sendto_server_id, static_cast<int>(senders_.size()));
        return -1;
    }
    
    ZSender* sender = senders_[sendto_server_id];
    if (sender)
    {
        return SendMsg(sender, header, msg);
    }
    else
    {
        LOG_ERR("found NULL ZSender by sendto_server_id %d.", sendto_server_id);
        return -1;
    }
}

int ZServer::SendMsgByType( const int sendto_server_type, SMsgHeader* header, const google::protobuf::Message* msg )
{
    if (sendto_server_type < 0)
    {
        LOG_ERR("Wrong sendto_server_type %d. OUT OF index %d.", sendto_server_type,
            static_cast<int>(type_senders_.size()));
        return -1;
    }

    ZSender* sender = type_senders_[sendto_server_type];
    if (sender)
    {
        return SendMsg(sender, header, msg);
    }
    else
    {
        LOG_ERR("found NULL ZSender by sendto_server_type %d.", sendto_server_type);
        return -1;
    }
}

int ZServer::SendMsg( ZSender* sender, SMsgHeader* header, const google::protobuf::Message* msg )
{
    // 打印信息， 方便快速定位信息
    if (header->msg_id == 0)
    {
        LOG_ERR("send failed. %s with no msgid", msg->GetTypeName().c_str());
        return -1;
    }
    //header->seq = GET_MSG_SEQ();
    int ret = sender->Send(header, msg);
    if (ret < 0)
    {
        LOG_ERR("zmq error[%d]: %s", zmq_errno(), zmq_strerror(zmq_errno()));
        /// now just fail
        if (zmq_errno() != EAGAIN)
            ;        
        return -1;
    }
    // 消息统计
    auto& msgs_stats = send_msg_stats_[header->dst_server_id];
    auto& msg_stats = msgs_stats[header->msg_id];
    msg_stats.first += 1;
    msg_stats.second += header->length;

    return ret;
}

int ZServer::SendMsg( const int sendto_server_id, SMsgHeader* msg )
{
    if (sendto_server_id < 0 )
    {
        LOG_ERR("Wrong sendto_server_id %d. OUT OF index %d.", sendto_server_id, static_cast<int>(senders_.size()));
        return -1;
    }

    ZSender* sender = senders_[sendto_server_id];
    if (sender)
    {
        return SendMsg(sender, msg);
    }
    else
    {
        LOG_ERR("found NULL ZSender by sendto_server_id %d.", sendto_server_id);
        return -1;
    }
}

int ZServer::SendMsgByType( const int sendto_server_type, SMsgHeader* msg )
{
    if (sendto_server_type < 0)
    {
        LOG_ERR("Wrong sendto_server_type %d. OUT OF index %d.", 
                    sendto_server_type, static_cast<int>(type_senders_.size()));
        return -1;
    }

    ZSender* sender = type_senders_[sendto_server_type];
    if (sender)
    {
        return SendMsg(sender, msg);
    }
    else
    {
        LOG_ERR("found NULL ZSender by sendto_server_type %d.", sendto_server_type);
        return -1;
    }
}

int ZServer::SendMsg( ZSender* sender, SMsgHeader* msg )
{
	if (this->server_status() == ZServerStatus::ZServerStatus_STOP )
	{
		LOG_INFO("Failed to send msg,server already stop");
		return -1;
	}
    int ret = sender->Send(msg);
    if (ret < 0)
    {
        LOG_ERR("zmq error[%d]: %s", zmq_errno(), zmq_strerror(zmq_errno()));
        /// now just fail
        if (zmq_errno() != EAGAIN)
            ;        
        return -1;
    }
    return ret;
}

void ZServer::CheckReceiverReadable( int32 server_id )
{
    asio_ref_zsockets_[server_id]->async_read_some(boost::asio::null_buffers(), 
        [=](const boost::system::error_code& error, int32 length)
    {
        this->HandleReceiverReadable(server_id, error);
    });
}

void ZServer::HandleReceiverReadable( int32 server_id, const boost::system::error_code& ec )
{
    if (!ec)
    {
        auto socket = receivers_[server_id];
        int count = 0;
        while (true)
        {
            int size = socket->ReceiveMessage();
			if (size > 0)
			{
				socket->HandlerReceived();
			}
            else
                break;
            // 公平性调度
            enum {fair_play_num = 1024};
            if (++count >= fair_play_num)
            {
                time_engine().post(boost::bind(&ZServer::HandleReceiverReadable, this, server_id, ec));
                return;
            }
        }
        CheckReceiverReadable(server_id);
    }
    else
    {
        if (ec.value() != boost::asio::error::operation_aborted)
        {
            LOG_ERR("timer ec %d: %s", ec.value(), ec.message().c_str());
        }
    }
    
}

void ZServer::OnStart(const std::function<bool ()>&& func)
{
    start_functions_.push_back(func);
}

void ZServer::CallLater( int32 call_after_ms, const std::function<void ()>&& func )
{
    boost::shared_ptr<boost::asio::deadline_timer> timer(new boost::asio::deadline_timer(TIME_ENGINE));
    timer->expires_from_now(boost::posix_time::millisec(call_after_ms));
    timer->async_wait([timer, func] (const boost::system::error_code& ec) {
        if (!ec)
        {
            func();
        }
        else
        {
            if (ec != boost::asio::error::operation_aborted)
                LOG_ERR("timer error %d: %s", ec.value(), ec.message().c_str());
        }
    });
}

void ZServer::ValidateServerSender( int32 server_id, bool success)
{
    if (success)
    {
        auto it = std::find_if(check_sender_flags_.begin(), check_sender_flags_.end(), 
            [=](const std::pair<int,int>& v){return v.first == server_id;});
        if ( it != check_sender_flags_.end())
        {
            if (it->second <= 0)
                it->second = 1;
        }
        HandShakeServers();
    }
    else
    {
        deadline_timer_.expires_from_now(boost::posix_time::seconds(1));
        deadline_timer_.async_wait(boost::bind(&ZServer::CheckHandShakeServers, this, boost::asio::placeholders::error));
    }
}

void ZServer::StartHandShakeServers()
{
    LOG_INFO("Server start handshake ...");
    HandShakeServers();
    deadline_timer_.expires_from_now(boost::posix_time::seconds(1));
    deadline_timer_.async_wait(boost::bind(&ZServer::CheckHandShakeServers, this, boost::asio::placeholders::error));    
}

void ZServer::HandShakeServers()
{
    if (handshake_str_.empty())
    {
        LOG_ERR("handshake str is empty.");
        Stop();
        return;
    }

    for (auto it=check_sender_flags_.begin(); it!=check_sender_flags_.end(); ++it )
    {
        if (it->second <= 0)
        {
            SendMsg(it->first, const_cast<SMsgHeader*>(reinterpret_cast<const SMsgHeader*>(handshake_str_.data())));
            break;
        }
    }
}

bool ZServer::CheckHandShakeServers( const boost::system::error_code& ec )
{
    if (!ec)
    {
        for (auto it=check_sender_flags_.begin(); it!=check_sender_flags_.end(); ++it)
        {
            if (it->second != 1)
            {            
                it->second -= 1;
                // TODO(zen): 5000 ? count
                if (it->second >= -5000)
                {
                    LOG_INFO("Waiting server %d for %d times...", it->first, -it->second);
                    deadline_timer_.expires_from_now(boost::posix_time::seconds(1));
                    deadline_timer_.async_wait(boost::bind(&ZServer::CheckHandShakeServers, this, boost::asio::placeholders::error));
                }
                else
                {
                    LOG_ERR("CenterServer failed connect to server %d.", it->first);
                    LOG_ERR("Waited %d sec. Stopping...", -it->second);
                    ForceStop();
                }
                HandShakeServers();
                return false;
            }
        }
        LOG_INFO("%s %d handshake check success.", server_name().c_str(), server_id());

        on_finish_handshake_func_();

        deadline_timer_.expires_from_now(boost::posix_time::millisec(10));
        deadline_timer_.async_wait(boost::bind(&ZServer::SecondTimerHandler, this, boost::asio::placeholders::error));

        return true;
    }
    else
    {
        if (ec.value() != boost::asio::error::operation_aborted)
        {
            LOG_ERR("timer ec %d: %s", ec.value(), ec.message().c_str());
        }
        return false;
    }
}

void ZServer::SecondTimerHandler( const boost::system::error_code& ec )
{
    if (!ec)
    {
        deadline_timer_.expires_from_now(boost::posix_time::millisec(10));
        deadline_timer_.async_wait(boost::bind(&ZServer::SecondTimerHandler, this, boost::asio::placeholders::error));
        boost::chrono::system_clock::now();
        time_engine_.Update();

        static auto last_trigger_ts = time_engine_.time_ms64();
        if (time_engine_.time_ms64() - last_trigger_ts >= 100)
        {
            last_trigger_ts = time_engine_.time_ms64();

            struct tm local_tm;
            time_t time_sec = TIME_ENGINE.time_sec();
            localtime_r( &time_sec, &local_tm );
            static auto last_trigger_sec = 0;
            if (local_tm.tm_sec != last_trigger_sec)
            {
                last_trigger_sec = local_tm.tm_sec;

                for (auto it=second_timer_functions_.begin(); it!=second_timer_functions_.end(); ++it)
                {
                    auto& functor = *it;
                    functor();
                }
            }
        }
    }
    else
    {
        if (ec.value() != boost::asio::error::operation_aborted)
        {
            LOG_ERR("timer ec %d: %s", ec.value(), ec.message().c_str());
        }
    }
}

/*void ZServer::Daemon()
{
#ifndef _WIN32
#include <unistd.h>
    time_engine_.notify_fork(boost::asio::io_service::fork_prepare);
    int32 ret = daemon(0, 0);
    if (ret != 0)
    {
        LOG_ERR("daemon failed. %d %s", errno, strerror(errno));
    }
    time_engine_.notify_fork(boost::asio::io_service::fork_child);
#endif // _WIN32
}*/

void ZServer::PrintMsg(SMsgHeader* s_msg, const bool IsSend)
{
	if (!trace_msg_flag_) {
		return;
	}
	if (s_msg == nullptr)
	{
		return;
	}
	const std::string s_note = (IsSend ? "SendMsg" : "RevMsg");
	const auto msg_names = ZSERVER.msg_names();
	if (msg_names != nullptr && s_msg->msg_id < INNER_MSG_START_ID)
	{
		const std::string &MsgName = (*msg_names)[s_msg->msg_id];
		google::protobuf::Message* protobuf_msg = z::util::CreateMessageByTypeName(MsgName);
		if (protobuf_msg != nullptr) {
			if (protobuf_msg->ParseFromArray(s_msg + 1, s_msg->length)) {
				PrintMsg(s_msg, protobuf_msg, IsSend);
			}
			else {
				LOG_ERR("%s[MsgID = %d][Len = %d], cant serialize ", s_note.c_str(), s_msg->msg_id, s_msg->length);
			}
			delete protobuf_msg;
			protobuf_msg = nullptr;
		}
	}
}
void ZServer::PrintMsg(SMsgHeader* header, const google::protobuf::Message* protobuf_msg, const bool IsSend)
{
	if (!trace_msg_flag_) {
		return;
	}
	const std::string s_note = (IsSend ? "SendMsg" : "RevMsg");
	if (header == nullptr || protobuf_msg == nullptr)
	{
		return;
	}
	if (header->msg_id < INNER_MSG_START_ID) {
		const std::string JsonString = z::util::ConvertMessageToJson(protobuf_msg);
		LOG_TRACE("%s[from = %d][to = %d][%s], %s ",s_note.c_str(), header->src_server_id,header->dst_server_id, protobuf_msg->GetTypeName().c_str(), JsonString.c_str());
	}
}

bool ZServer::HttpPostReq(char const* host, char const*port, char const*target, const std::string &content, std::function<void(const std::string&, bool)> callback)
{
	if (host == nullptr || port == nullptr || target == nullptr)
	{
		LOG_FATAL("Http Post host == nullptr or port == nullptr or target == nullptr ");
		return false;
	}
	const int version = 10;
	auto http =  std::make_shared<HttpClientSession>(time_engine_);
	if (http == nullptr) {
		return false;
	}
	http->run(host, port, target, content, version, callback);
	return true;
}

void ZServer::RecodRevTime(const int sender_id)
{
	const int32 src_sender_id = sender_id;
	const auto src_sender = this->GetSender(src_sender_id);
	if (src_sender == nullptr)
	{
		LOG_FATAL(" Sender %d ,server_type = %d == null ", src_sender_id, SERVER_TYPE(src_sender_id));
		return;
	}
	else {
		src_sender->set_rev_msg_time(TIME_ENGINE.time());
	}
}

void TimeEngine::Init()
{
    time_t t_now = ::time(nullptr);
    struct tm tm_l, tm_g;
    localtime_r(&t_now, &tm_l);
    gmtime_r(&t_now, &tm_g);
    time_t ts_l = mktime(&tm_l);
    time_t ts_g = mktime(&tm_g);    

    int time_zone = static_cast<int32>((ts_l - ts_g) / 3600);

    // 不知道到底时区怎么分的, 有的最大+12 有的+13 还有+14????
    if (!(time_zone > 15 || time_zone < -12))
    {
        time_zone_sec_ = time_zone * 3600;
    }
    else
    {
        LOG_ERR("time zone caculate error.");
        time_zone_sec_ = 8 * 3600;
    }

    timestamp_ms_ = boost::chrono::duration_cast<boost::chrono::milliseconds>(
        boost::chrono::system_clock::now().time_since_epoch()).count();
}

void TimeEngine::Update()
{
    timestamp_ms_ = boost::chrono::duration_cast<boost::chrono::milliseconds>(
        boost::chrono::system_clock::now().time_since_epoch()).count();
}


void SendSMsgBySid(int sendto_server_id, int32 msg_id, SMsgHeader& header, google::protobuf::Message& body)
{
    header.msg_id = msg_id;
    ZSERVER.SendMsg(sendto_server_id, &header, &body);
}

void SendSMsgByType(int sendto_server_type, int32 msg_id, SMsgHeader& header, google::protobuf::Message& body)
{
    header.msg_id = msg_id;
    ZSERVER.SendMsgByType(sendto_server_type, &header, &body);
}

} //namespace net
} //namespace z
