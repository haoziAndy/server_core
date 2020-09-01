#ifndef Z_NET_Z_SERVER_H
#define Z_NET_Z_SERVER_H

#include "z/net/msg_header.h"


#define SERVER_TYPE_BITS 6
#define SERVER_INDEX_BITS 10
#define MAX_SERVER_TYPE ((1 << (SERVER_TYPE_BITS)) - 1)
#define MAX_SERVER_INDEX ((1 << (SERVER_INDEX_BITS)) - 1)
#define MAX_SERVER_ID ((1 << (SERVER_TYPE_BITS + SERVER_INDEX_BITS)) - 1)

inline uint32 SERVER_ID(uint32 server_type, uint32 server_index)
{
    BOOST_ASSERT(server_type <= MAX_SERVER_TYPE);
    BOOST_ASSERT(server_index <= MAX_SERVER_INDEX);
    return static_cast<uint32>(((server_type & MAX_SERVER_TYPE) << SERVER_INDEX_BITS) | (server_index & MAX_SERVER_INDEX));
}
inline uint32 SERVER_TYPE(uint32 server_id)
{
    BOOST_ASSERT(server_id <= MAX_SERVER_ID);
    return static_cast<uint32>((server_id >> SERVER_INDEX_BITS) & MAX_SERVER_TYPE);
}
inline uint32 SERVER_INDEX(uint32 server_id)
{
    BOOST_ASSERT(server_id <= MAX_SERVER_ID);
    return static_cast<uint32>(server_id & MAX_SERVER_INDEX);
}



namespace z {
namespace net {

class ZReceiver;
class ZSender;
class ISMsgHandler;
struct SMsgHeader;

// 为了支持 graceful restart
enum ZServerStatus
{
    ZServerStatus_DEFAULT = 0,
    ZServerStatus_INIT,          // 启动
	ZServerStatus_LOAD_DATA,     //加载数据
    ZServerStatus_WORKING,       // 工作
    ZServerStatus_STOP,          // 停止

};

class TimeEngine : public boost::asio::io_service
{
public:
    TimeEngine()
        : time_zone_sec_(0)
    {
        Init();
    }
    void Update();

    int64 time() const {return timestamp_ms_;}
    int64 time_ms64() const {return time(); }
    int32 time_ms() const {return static_cast<int32>(time()); }
    int32 time_sec() const {return static_cast<int32>(time_sec64()); }
    int64 time_sec64() const {return time() / 1000; }
    int32 time_zone_sec() const { return time_zone_sec_;};
    
    int32 LocalDayDiff(int32 ts1, int32 ts2) const
    {
        int day_diff = (ts1 + time_zone_sec()) / (24 * 60 * 60) - (ts2 + time_zone_sec()) / (24 * 60 * 60);
        return day_diff;
    }
    bool IfTriggerLocalTimer(int32 last_trigger_time, int32 timer_period, int32 timer_trigger_at) const
    {
        int local_sec = time_sec() + time_zone_sec();

        int diff_count = local_sec / timer_period -
            (last_trigger_time + time_zone_sec()) / timer_period;
        if (diff_count <= 0)
        {
            return false;
        }
        else if (diff_count == 1)
        {
            return timer_trigger_at <= local_sec % timer_period;
        }
        else
        {
            return true;
        }
    }
private:
    void Init();
private:
    int32 time_zone_sec_;
    int64 timestamp_ms_;
};

class ZServer
{
    typedef std::map<int, ZReceiver*> Receivers;
    typedef std::map<int, ZSender*> Senders;

    enum {server_time_piece_ms = 10};
    ZServer()
        : is_server_shutdown_(false)
        , is_delegate_looping_(false)
        , server_id_(0)
        , server_type_(0)
        , server_status_(0)
        , work_(time_engine_)
        , signals_(time_engine_)
        , deadline_timer_(time_engine_)
        , msg_names_(nullptr)
    {
		// init current_path
		current_path_ = boost::filesystem::current_path().generic_string() + "/";
	}

public:

    ~ZServer()
    {}

    //int Init(const char* server_config_file, int server_group_id, int server_type, ISMsgHandler* msg_handler);
    int Init(int server_id, int server_type, const std::string& server_name, 
        const std::string& listen_str, const std::string& conn_str, ISMsgHandler* msg_handler);

    int Destroy();

    void Run();

    void Stop();
    void ForceStop();

	bool HttpPostReq(char const* host, char const*port, char const*target, const std::string &content, std::function<void(const std::string&, bool)> callback);

    void RegisterCleanFunctor(const boost::function<void()>&& functor)
    {
        clean_functions_.push_back(functor);
    }

    void RegisterTimerFunctor(const boost::function<void()>&& functor)
    {
        second_timer_functions_.push_back(functor);
    }

    // 服务器启动时候自动调用
    void OnStart(const std::function<bool ()>&& func);
    
    // 回调触发使用延时防止一些异常错误
    void CallLater(int32 call_after_ms, const std::function<void ()>&& func);

    ////============================ 服务器握手 ========================//
    /// 收到 server hand shake verify 消息
    void ValidateServerSender(int32 server_id, bool success);


    /// 发送消息, 需要序列化
    int SendMsg(const int sendto_server_id, SMsgHeader* header, const google::protobuf::Message* msg);
    /// 发送给某个类型服务器
    int SendMsgByType(const int sendto_server_type, SMsgHeader* header, const google::protobuf::Message* msg);    
    int SendMsg(ZSender* sender, SMsgHeader* header, const google::protobuf::Message* msg);

    /// 转发消息, 已经序列化
    int SendMsg(const int sendto_server_id, SMsgHeader* msg);
    /// 发送给某个类型服务器, 已经序列化
    int SendMsgByType(const int sendto_server_type, SMsgHeader* msg);
    int SendMsg(ZSender* sender, SMsgHeader* msg);


    TimeEngine& time_engine() { return time_engine_; }

    ZReceiver* AddReceiver(const int server_id, const std::string& listen_str, ISMsgHandler* msg_handler);
    
    ZSender* AddSender(const int sendto_server_id, const std::string& connect_str, int server_type, const std::string& server_name, bool need_verify = false);
    ZSender* GetSender(const int sendto_server_id) const
    {
		auto tem = senders_.find(sendto_server_id);
		if (sendto_server_id < 0 || tem == senders_.end())
			return nullptr;
		return tem->second;
    };
    const std::vector<ZSender*> GetSendersByType(uint32 server_type) const;

	const std::map<int,ZSender*> GetAllSenders() const {
		return senders_;
	}

    int server_id() const {return server_id_;}
    int server_type() const {return server_type_;}
    const std::string& server_name() const {return server_name_;}
    const std::string& server_conn_str() const {return server_conn_str_;}
    int server_status() const {return server_status_;}
    void set_server_status(int32 status) {server_status_ = status;}
    ISMsgHandler* msg_handler() const {return msg_handler_;}
    void set_handshake_str(const std::string& handshake_str) {handshake_str_ = handshake_str;}
    void set_handshake_str(const char* data, int32 len) {handshake_str_.assign(data, len);}
    void set_on_finish_handshake_func(boost::function<void ()>&& func) {on_finish_handshake_func_ = func;}

    const std::string& current_path() {return current_path_;}
    // 后台化, linux 下有效
    //void Daemon();

    std::unordered_map<int32, const std::string>* msg_names() const {return msg_names_;}
    void set_msg_names(std::unordered_map<int32, const std::string>* name_list) { msg_names_ = name_list;}

	void PrintMsg(SMsgHeader* s_msg,const bool IsSend);
	void PrintMsg(SMsgHeader* header, const google::protobuf::Message* protobuf_msg, const bool IsSend);

	void RecodRevTime(const int sender_id);

	void set_trace_msg_flag(bool trace_msg_flag) { trace_msg_flag_ = trace_msg_flag; }
	bool trace_msg_flag() const { return trace_msg_flag_; }

    const std::unordered_map<int32, std::unordered_map<int32, std::pair<int32, int64>>>& send_msg_stats() const {return send_msg_stats_;}
    const std::unordered_map<int32, std::unordered_map<int32, std::pair<int32, int64>>>& recv_msg_stats() const {return recv_msg_stats_;}
private:
    int AddPollItem(ZReceiver* receiver);    

private:
    // 开始握手
    void StartHandShakeServers();
    /// 发送 hand shake
    void HandShakeServers();
    /// 定时器结束, 检查是否所有sender都收到了响应
    bool CheckHandShakeServers(const boost::system::error_code& ec);
    // 定时器功能
    void SecondTimerHandler(const boost::system::error_code& ec);

private:
    void DelegateLoop();
    // call by DelegateLoop
    void CheckReceiverReadable(int32 server_id);
    void HandleReceiverReadable(int32 server_id, const boost::system::error_code& ec);

private:
    std::vector<zmq_pollitem_t> poll_items_;
    std::vector<ZReceiver*> ref_poll_receivers_; // ref by index
    std::unordered_map<int32, boost::shared_ptr<boost::asio::ip::tcp::socket>> asio_ref_zsockets_;

    bool is_server_shutdown_;
    bool is_delegate_looping_;

    //int32 server_group_id_;
    int32 server_id_;
    int32 server_type_;
    std::string server_name_;
    std::string server_conn_str_;
    int32 server_status_;
    ISMsgHandler* msg_handler_;

    Receivers receivers_;
    Senders senders_;        ///< 以server_id为index
    Senders type_senders_;    ///< 以类型为index

    TimeEngine time_engine_;
    boost::asio::io_service::work work_;

    /// The signal_set is used to register for process termination notifications.
    boost::asio::signal_set signals_;
    boost::asio::deadline_timer deadline_timer_;
    
    std::vector<boost::function<void ()>> second_timer_functions_;

    std::vector<std::pair<int, int>> check_sender_flags_;

    std::vector<boost::function<void ()> > clean_functions_;

    std::vector<std::function<bool ()>> start_functions_;

    std::string handshake_str_;
    boost::function<void ()> on_finish_handshake_func_;
    std::string current_path_;

    // <server_id, <msg_id, <count, total_length>>>
    std::unordered_map<int32, std::unordered_map<int32, std::pair<int32, int64>>> send_msg_stats_;
    std::unordered_map<int32, std::unordered_map<int32, std::pair<int32, int64>>> recv_msg_stats_;

    // msg_id, msg_name 对应
    std::unordered_map<int32, const std::string>* msg_names_;

    bool trace_msg_flag_;

    DECLARE_SINGLETON(ZServer);
};

#define ZSERVER Singleton<z::net::ZServer>::instance()
#define TIME_ENGINE ZSERVER.time_engine()

void SendSMsgBySid(int sendto_server_id, int32 msg_id, SMsgHeader& header, google::protobuf::Message& body);

void SendSMsgByType(int sendto_server_type, int32 msg_id, SMsgHeader& header, google::protobuf::Message& body);


}; //namespace net
}; //namespace z


#endif //Z_NET_Z_SERVER_H

