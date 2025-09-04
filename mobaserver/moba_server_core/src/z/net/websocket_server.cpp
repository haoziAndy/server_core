#include "stdafx.h"
#include "websocket_server.h"
#include "websocket_connection.h"
#include "msg_header.h"


namespace z {
namespace net {

bool WebsocketServer::Init( const std::string& address, const std::string& port, ICMsgHandler* handler){
    if (CCServer::Init(address, port, handler) < 0){
        return false;
    }

#ifdef ENABLE_WEBSOCKET_SSL
    try {

        ssl_ctx_.set_password_callback([](std::size_t, boost::asio::ssl::context_base::password_purpose){
            return "test";
        });

        ssl_ctx_.set_options(
            boost::asio::ssl::context::default_workarounds |
            boost::asio::ssl::context::no_sslv2 |
            boost::asio::ssl::context::single_dh_use);

        // 加载证书链文件（PEM格式）
        ssl_ctx_.use_certificate_chain_file("./pem/cert.pem");
        
        // 加载私钥文件（PEM格式）
        ssl_ctx_.use_private_key_file("./pem/key.pem", boost::asio::ssl::context::pem);
        
        ssl_ctx_.use_tmp_dh_file("./pem/dh.pem");
        // 验证私钥与证书匹配
        SSL_CTX_check_private_key(ssl_ctx_.native_handle());

    } catch (const std::exception& e) {
        LOG_ERR("Certificate loading failed: %s", e.what());
        return false;
    }
#endif

    return true;
}

}// namespace net
}// namespace z