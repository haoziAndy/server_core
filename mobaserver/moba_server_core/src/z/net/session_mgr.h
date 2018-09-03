#ifndef NET_SESSION_MGR_H_
#define NET_SESSION_MGR_H_

namespace z {
namespace net {

struct SessionData
{
    SessionData()
        : last_active_sec(0)
        , openid(0)
        , sid(0)
        , persistence_mode(0)
        , checksum(0)
    {}

    std::string session_key;
    int64 last_active_sec;
    int64 openid;
    int32 sid;
    int32 persistence_mode;             // 0短连接, 1 长连接
    int32 checksum;
};

class SessionMgr
{
public:
    // 生成新的session, 在登录后生成, 所以openid必须有
    boost::shared_ptr<SessionData> CreateSessionData(int64 openid);

    // 获取, 同时更新last_active_sec
    boost::shared_ptr<SessionData> GetSessionData(int64 openid);

    // 校验checksum
    bool IsChecksumValid(int64 openid, int32 checksum, const std::string& crypt_str);

    // 移除过期session
    void RemoveExpiredSessionData();

    // 销毁 session_data
    void DestroySessionData(int64 openid);
private:
    // 生成session key 随机字符串16B
    std::string GenerateSessionKey();

    // 检查session_key 是否有效
    bool IsSessionKeyValid(const std::string& session_key) const
    {
        return used_session_key_list_.find(session_key) == used_session_key_list_.end();
    }
    // 生成随机字符串 大小写字母 数字 符号组合
    std::string GenRandomStr(int32 length);
private:
    // 
    std::unordered_map<int64, boost::shared_ptr<SessionData>> session_list_;

    // 已经使用的session_key
    std::unordered_map<std::string, int32> used_session_key_list_;

    // 最近最常使用, 检查session过期用 <last_active_sec, <openid, session_data>>
    std::map<int64, std::unordered_map<int64, boost::shared_ptr<SessionData>>> lru_session_list_;
};


} // namespace net
} // namespace z

#define SESSION_MGR Singleton<z::net::SessionMgr>::instance()

#endif // NET_SESSION_MGR_H_
