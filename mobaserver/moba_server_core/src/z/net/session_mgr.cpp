#include "stdafx.h"
#include "session_mgr.h"
#include "z_server.h"

namespace z {
namespace net {

static const boost::shared_ptr<SessionData> empty_ptr;

std::string SessionMgr::GenRandomStr( int32 length )
{
    const static std::string chars(
        "abcdefghijklmnopqrstuvwxyz"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "1234567890"
        "!@#$%^&*()"
        "`~-_=+[{]}\\|;:'\",<.>/? ");

    static boost::random::random_device rng;
    static boost::random::uniform_int_distribution<> index_dist(0, chars.size() - 1);
    
    std::string output;
    output.resize(length);
    char* buff = const_cast<char*>(output.c_str());
    for (int i = 0; i < length; ++i) 
    {
        buff[i] = chars[index_dist(rng)];
    }
    return output;
}

std::string SessionMgr::GenerateSessionKey()
{
    enum { session_key_length = 16};
    enum { max_repeated_count = 1024};
    for (int i = 0; i < max_repeated_count; ++i)
    {
        auto key = GenRandomStr(session_key_length);
        if (IsSessionKeyValid(key))
            return key;
    }
    
    return "ERROR_SESSION_KEY";
}

boost::shared_ptr<SessionData> SessionMgr::GetSessionData( int64 openid )
{
    auto it = session_list_.find(openid);
    if (it != session_list_.end())
    {
        auto session_data = it->second;
        // 删除老active_sec
        auto& o_lru_list = lru_session_list_[session_data->last_active_sec];
        o_lru_list.erase(openid);
        session_data->last_active_sec = TIME_ENGINE.time_sec64();
        auto& lru_list = lru_session_list_[session_data->last_active_sec];
        lru_list[session_data->openid] = session_data;

        return session_data;
    }
    else
    {
        return empty_ptr;
    }
}

boost::shared_ptr<SessionData> SessionMgr::CreateSessionData(int64 openid)
{
    auto session_data = ZPOOL_NEW_SHARED(SessionData);
    session_data->session_key = GenerateSessionKey();
    session_data->openid = openid;
    session_data->last_active_sec = TIME_ENGINE.time_sec64();
    // 如果已有, 销毁原有
    DestroySessionData(openid);

    session_list_[openid] = session_data;
    auto& lru_list = lru_session_list_[session_data->last_active_sec];
    lru_list[openid] = session_data;

    used_session_key_list_[session_data->session_key] = 1;

    return session_data;
}

void SessionMgr::DestroySessionData( int64 openid )
{
    auto it = session_list_.find(openid);
    if (it != session_list_.end())
    {
        auto session_data = it->second;
        // 删除老active_sec
        auto& o_lru_list = lru_session_list_[session_data->last_active_sec];
        o_lru_list.erase(openid);
        session_list_.erase(it);

        used_session_key_list_.erase(session_data->session_key);
    }
    else
    {
        return;
    }
}

bool SessionMgr::IsChecksumValid( int64 openid, int32 checksum, const std::string& crypt_str )
{
    auto session_data = GetSessionData(openid);
    if (session_data == nullptr)
        return false;
    // 防止重放攻击
    //if (checksum == session_data->checksum)
    //    return false;
    // auto key = z::util::MD5(session_data->session_key + std::to_string(checksum)).substr(0, sizeof(((CMsgHeader*)0)->key));
    auto& key  = session_data->session_key;
    if (key == crypt_str)
        return true;
    else
        return false;
}

void SessionMgr::RemoveExpiredSessionData()
{
    enum {session_expired_sec = 9000};
    std::set<int64> expired_session;
    for (auto it = lru_session_list_.begin(); it != lru_session_list_.end(); ++it)
    {
        auto lru_sec = it->first;
        auto& session_list = it->second;
        if (lru_sec > TIME_ENGINE.time_sec64() - session_expired_sec)
            break;
        for (auto sit = session_list.begin(); sit != session_list.end(); ++sit)
        {
            expired_session.insert(it->first);
        }
    }
    for (auto it = expired_session.begin(); it != expired_session.end(); ++it)
    {
        DestroySessionData(*it);
    }
}




} // namespace net
} // namespace z

