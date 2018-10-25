#include "stdafx.h"
#include "session_mgr.h"

#ifdef _WIN32
#ifdef _DEBUG
#    pragma comment(lib, "libzmq.lib")
#else
#    pragma comment(lib, "libzmq.lib")
#endif // _DEBUG

#pragma comment(lib, "libzcommon.lib")
#pragma comment(lib, "libzcontrib.lib")
#pragma comment(lib, "libprotobuf.lib")

#endif //_WIN32

 
int main()
{
    std::cout << "Hello World." << std::endl;
//     if (!UDPSERVER.Init(8432, 1024, nullptr))
//     {
//         LOG_ERR("init failed.");
//         return -1;
//     }
// 
//     UDPSERVER.Run();
//     UDPSERVER.Destroy();
//     for (int i = 0; i < 32; i++)
//     {
//         std::cout << SESSION_MGR.GenerateSessionKey() << std::endl;
//     }
//     
    auto session_data = SESSION_MGR.CreateSessionData(111);
    auto str = session_data->session_key.substr(0, 32);
    std::string str2(str.c_str(), 2);
    return EXIT_SUCCESS;
}
