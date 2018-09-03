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
    int32 persistence_mode;             // 0������, 1 ������
    int32 checksum;
};

class SessionMgr
{
public:
    // �����µ�session, �ڵ�¼������, ����openid������
    boost::shared_ptr<SessionData> CreateSessionData(int64 openid);

    // ��ȡ, ͬʱ����last_active_sec
    boost::shared_ptr<SessionData> GetSessionData(int64 openid);

    // У��checksum
    bool IsChecksumValid(int64 openid, int32 checksum, const std::string& crypt_str);

    // �Ƴ�����session
    void RemoveExpiredSessionData();

    // ���� session_data
    void DestroySessionData(int64 openid);
private:
    // ����session key ����ַ���16B
    std::string GenerateSessionKey();

    // ���session_key �Ƿ���Ч
    bool IsSessionKeyValid(const std::string& session_key) const
    {
        return used_session_key_list_.find(session_key) == used_session_key_list_.end();
    }
    // ��������ַ��� ��Сд��ĸ ���� �������
    std::string GenRandomStr(int32 length);
private:
    // 
    std::unordered_map<int64, boost::shared_ptr<SessionData>> session_list_;

    // �Ѿ�ʹ�õ�session_key
    std::unordered_map<std::string, int32> used_session_key_list_;

    // ����ʹ��, ���session������ <last_active_sec, <openid, session_data>>
    std::map<int64, std::unordered_map<int64, boost::shared_ptr<SessionData>>> lru_session_list_;
};


} // namespace net
} // namespace z

#define SESSION_MGR Singleton<z::net::SessionMgr>::instance()

#endif // NET_SESSION_MGR_H_
