#ifndef NET_MSG_HANDLER_H
#define NET_MSG_HANDLER_H

namespace z {
namespace net {

struct SMsgHeader;
struct CMsgHeader;
class CConnection;
class HConnection;
class UConnection;
namespace http {
    struct RequestUri;
}

/// 消息处理器接口
class ISMsgHandler
{
public:
    virtual ~ISMsgHandler()
    {}

    //virtual int OnMessage(SMsgHeader* msg) const = 0;

    /// @param recv_tick unit ms
    virtual int OnMessage(SMsgHeader* msg, int64 recv_tick) = 0;
};

/// 客户端消息处理器接口,
/// 客户端消息接收和客户消息发送 分别实现
class ICMsgHandler
{
public:
    virtual ~ICMsgHandler()
    {}
    // 特殊消息处理, 像账户登录, 角色登录, 需要修改conn状态的一些消息. 
    // 触发在转发给zservice之后
    // -1 表示出错, 需要关闭客户端连接
    virtual int OnMessage(CConnection* conn, CMsgHeader* msg) = 0;
    // 客户端断线
    virtual void OnClientDisconnect(CConnection* conn) {}
};

/// http 请求处理器接口
class IHttpRequestHandler
{
public:
    virtual ~IHttpRequestHandler()
    {}

    virtual std::string OnRequest(const std::string& request_body) = 0;
};

/// 客户端消息处理器接口,
/// 客户端消息接收和客户消息发送 分别实现
class IUMsgHandler
{
public:
    virtual ~IUMsgHandler()
    {}
    // 特殊消息处理, 像账户登录, 角色登录, 需要修改conn状态的一些消息. 
    // 触发在转发给zservice之后
    // -1 表示出错, 需要关闭客户端连接
    virtual int OnMessage(UConnection* conn, CMsgHeader* msg) = 0;
    // 客户端断线
    virtual void OnClientDisconnect(UConnection* conn) {}
};

enum LoginStatus
{
    LoginStatus_DEFAULT,
    LoginStatus_ACCOUNT_LOGIN,
    LoginStatus_PLAYER_CREATE,
    LoginStatus_PLAYER_LOGIN,
    LoginStatus_PLAY_GAME,
    LoginStatus_CLOSING,
    LoginStatus_CLOSED,
};

} //namespace net
} //namespace z

#endif //NET_MSG_HANDLER_H
