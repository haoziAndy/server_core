#include "stdafx.h"
#include "z/common/logger.h"
#include <log4cplus/logger.h>
#include <log4cplus/fileappender.h>
#include <log4cplus/layout.h>
#include <log4cplus/ndc.h>
#include <log4cplus/helpers/loglog.h>
#include <log4cplus/helpers/property.h>
#include <log4cplus/loggingmacros.h>
#include <log4cplus/initializer.h>
#include "z/common/core_dump.h"

#ifdef _WIN32
#ifdef _DEBUG
//#    pragma comment(lib, "libzmq.lib")
//#    pragma comment(lib, "libprotobuf.lib")
#else
#    pragma comment(lib, "libzmq.lib")
#    pragma comment(lib, "libprotobufd.lib")
#endif // _DEBUG

#pragma comment(lib, "libzcommon.lib")
#pragma comment(lib, "libzcontrib.lib")
#pragma comment(lib, "log4cplusD.lib")
#pragma comment(lib, "exception_handler.lib")
#pragma comment(lib, "crash_generation_client.lib")
#pragma comment(lib, "common.lib")
#endif //_WIN32
/*class CostTick
{
public:
    CostTick()
    {
        tick_ = boost::chrono::duration_cast<boost::chrono::milliseconds>(
            boost::chrono::system_clock::now().time_since_epoch()).count();
    }
    int64 tick() const 
    {
        return tick_;
    }
    int64 cost_tick() 
    {
        auto cur_tick = boost::chrono::duration_cast<boost::chrono::milliseconds>(
            boost::chrono::system_clock::now().time_since_epoch()).count();
        auto cost = cur_tick - tick_;
        tick_ = cur_tick;
        return cost;
    }
    void Print() 
    {
        auto cur_tick = boost::chrono::duration_cast<boost::chrono::milliseconds>(
            boost::chrono::system_clock::now().time_since_epoch()).count();
        std::cout << "cost " << (cur_tick - tick_) << " ms." << std::endl;
        tick_ = cur_tick;
    }
private:
    int64 tick_;
};
int main()
{
    std::cout << "Hello world!\n" << std::endl;
    
    auto alloc_size = 8;
    auto alloc_count = 20;
    std::vector<void*> addr_list;
    for (auto i = 0; i < alloc_count; ++i)
    {
        auto addr = ZPOOL_MALLOC(8 << i);
        addr_list.push_back(addr);
    }
    std::cout << ZPOOL.PrintStats() << std::endl;
    for (auto it = addr_list.begin(); it != addr_list.end(); ++it)
    {
        auto addr = *it;
        ZPOOL_FREE(addr);
    }
    std::cout << ZPOOL.PrintStats() << std::endl;


    auto heat_count = 1000*10;
    std::unordered_set<void*> heat_addr_list;
    for (auto i = 0; i < heat_count; ++i)
    {
        auto addr = ZPOOL_MALLOC(i + 1);
        heat_addr_list.insert(addr);
    }
    std::cout << ZPOOL.PrintStats();
    for (auto it = heat_addr_list.begin(); it != heat_addr_list.end(); ++it)
    {
        auto addr = *it;
        ZPOOL_FREE(addr);
    }
    std::cout << ZPOOL.PrintStats();
    ZPOOL.set_need_stats(false);
    auto bench_count = 1024;// * 1024;
    CostTick tick;
    for (int j = 8; j < 1024 * 1024; j *= 2)
    {
        {
            for (auto i = 0; i < bench_count; ++i)
            {
                auto addr = ZPOOL_MALLOC(j);
                ZPOOL_FREE(addr);
            }
            auto cost_time = tick.cost_tick();
            std::cout << "bench palloc size " << j 
                << " count " << bench_count << ","
                <<" cost " << cost_time << " ms,"
                << "avg " << (bench_count * 1000.0 / cost_time) << " requests per sec"
                << std::endl; 
        }
        
        {
            std::vector<void*> list_addr;
            list_addr.reserve(bench_count);
            for (auto i = 0; i < bench_count; ++i)
            {
                auto addr = std::malloc(j);
                std::free(addr);
                list_addr.push_back(addr);
            }
            auto cost_time = tick.cost_tick();
            std::cout << "bench malloc size " << j 
                << " count " << bench_count << ","
                <<" cost " << cost_time << " ms,"
                << "avg " << (bench_count * 1000.0 / cost_time) << " requests per sec"
                << std::endl; 
        }
        
    }
    


    return 0;
}*/

void crash() {
	volatile int* a = (int*)(NULL);
	*a = 1;
}

int main() {
	log4cplus::initialize();
	log4cplus::Initializer initializer;
	LOGGER.Init("scene","./log", log4cplus::TRACE_LOG_LEVEL);
	CORE_DUMPER.Init("./log");
	crash();
	return 0;
}