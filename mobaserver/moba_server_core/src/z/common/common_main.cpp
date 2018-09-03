#include "stdafx.h"
#include "logger.h"
#include "random.h"
#include "error.h"

#ifdef _WIN32
#pragma comment(lib, "libzmq.lib")
#pragma comment(lib, "libprotobuf.lib")
#endif

int main()
{
    std::cout << "Common: Hello World." << std::endl;

//    LOG("ERROR", "ASDF");
    RANDOM(1,5);
    return 0;
}
