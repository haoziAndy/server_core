#include "stdafx.h"
#include "memory_pool.h"

#ifdef _WIN32
#ifndef snprintf
#define snprintf sprintf_s
#endif
#endif

namespace z {
namespace common {

static void LOG_ERR(const char* data)
{
    int len = strlen(data);
    std::fstream fs("./error_memory.log", std::ios::out | std::ios::app);
    if (!fs.is_open())
    {
        std::cout << "logger open file failed:" << "./error_memory.log" << std::endl;
        return;
    }
    struct tm local_tm;
    time_t time_sec = time(nullptr);
    localtime_r( &time_sec, &local_tm );
    char datetime_str[26];
    snprintf(datetime_str, sizeof(datetime_str) - 1, "%04d-%02d-%02d %02d:%02d:%02d ",
        local_tm.tm_year+1900, local_tm.tm_mon+1,
        local_tm.tm_mday, local_tm.tm_hour, local_tm.tm_min,
        local_tm.tm_sec);
    const static int time_str_len = strlen(datetime_str);
    fs.write(datetime_str, time_str_len);
    fs.write(data, len);
    if (data[len - 1] != '\n')
        fs.write("\n", 1);
    fs.flush();
}

GlobalMemPool::GlobalMemPool()
    : need_stats_(true)
{
    for (int i=0; i<max_array_size; ++i)
    {
        pools_[i] = new boost::pool<>(start_alloc_size << i);
    }
    thread_id_ = boost::this_thread::get_id();
}

GlobalMemPool::~GlobalMemPool()
{
    PurgeMemory();
}

int GlobalMemPool::GetArrayIndexBySize( int size )
{
    for (int i=0; i<max_array_size; ++i)
    {
        if (size <= (start_alloc_size << i))
            return i;
    }
    return -1;
}

void* GlobalMemPool::Malloc( const char* file, const int line, const char* func, int size )
{
    // 头尾各自4字节
    size += 4 + 4;
    int index = GetArrayIndexBySize(size);
    void* data;
    if (index >= 0) 
    {
        size = start_alloc_size << index;
        data = pools_[index]->malloc();
    }
    else
    {
        data = std::malloc(size);
    }

    if (data == nullptr)
    {
        LOG_ERR(boost::str(boost::format("malloc failed : %s %d %s %d") % file % line % func % size).c_str());
        std::bad_alloc exception;
        throw exception;
        return nullptr;
	}

    *reinterpret_cast<int*>(data) = size;
    *reinterpret_cast<int*>(reinterpret_cast<char*>(data) + size - 4) = (~size);
    
    AllocFrom alloc_from;
    alloc_from.line = line;
    alloc_from.file = file;
    alloc_from.func = func;
    alloced_data_.insert(std::make_pair(data, alloc_from));
    if (need_stats_)
        AddStats(alloc_from, size);
    
    return reinterpret_cast<char*>(data) + 4;
}

void GlobalMemPool::Free( void* const data , const char* file, const int line, const char* func )
{
    if (data == nullptr)
    {
        LOG_ERR(boost::str(boost::format("Try to free NULL: %s %d %s") % file % line % func).c_str());
        return;
    }

    void* addr = reinterpret_cast<char*>(data) - 4;

    auto it = alloced_data_.find(addr);
    if (it == alloced_data_.end())
    {
        LOG_ERR(boost::str(boost::format("Try to free unalloced from ZMEMPOOL: %s %d %s") % file % line % func).c_str());
        return;
    }

    int size = *reinterpret_cast<int*>(addr);
    int checksum = *reinterpret_cast<int*>(reinterpret_cast<char*>(addr) + size - 4);
    if (checksum != (~size))
    {
        LOG_ERR(boost::str(boost::format("ZMEMPOOL ERROR, memory overflow: %s %d %s") % file % line % func).c_str());
        return;
    }
// #if !defined NDEBUG
//     memset(addr, 0, size);
// #endif //  !defined NDEBUG

    int index = GetArrayIndexBySize(size);
    if (index >= 0)
    {
        if (!pools_[index]->is_from(addr))
        {
            LOG_ERR(boost::str(boost::format("Try to free data which ZMEMPOOL alloced but pool not recognized. %s %d %s") % file % line % func).c_str());
            return;
        }
        pools_[index]->free(addr);
    }
    else
    {
        std::free(addr);
    }

    if (need_stats_)
        AddStats(it->second, -size);

    alloced_data_.erase(it);
            
}

// void GlobalMemPool::Free( void* const data )
// {
//     void* addr = reinterpret_cast<char*>(data) - 4;
// 
//     auto it = alloced_data_.find(addr);
//     if (it == alloced_data_.end())
//     {
//         LOG_ERR(boost::str(boost::format("Try to free unalloced from ZMEMPOOL: %p") % data).c_str());
//         return;
//     }
// 
//     int size = *reinterpret_cast<int*>(addr);
// #if !defined NDEBUG
//     memset(addr, 0, size);
// #endif //  !defined NDEBUG
// 
//     int index = GetArrayIndexBySize(size);
//     if (index >= 0)
//     {
//         if (!pools_[index]->is_from(addr))
//         {
//             LOG_ERR("Try to free data which ZMEMPOOL alloced but pool not recognized.");
//             return;
//         }
//         pools_[index]->free(addr);
//     }
//     else
//     {
//         std::free(addr);
//     }
// 
//     AddStats(it->second, -size);
// 
//     alloced_data_.erase(it);
// }

void GlobalMemPool::PurgeMemory()
{
    for (auto it = alloced_stats_data_.begin(); it!=alloced_stats_data_.end(); ++it)
    {
        int lost_bytes =  it->second.first;
        if (lost_bytes == 0)
            continue;
        const AllocFrom& from = it->first;
        LOG_ERR(boost::str(boost::format("memory leak: %s %d %s(): %d Bytes not freed.") % from.file % from.line % from.func % lost_bytes).c_str());
    }
    for (auto it = alloced_data_.begin(); it != alloced_data_.end(); )
    {
        AllocFrom& from = it->second;
        void* alloc_data = it->first;        
        ++it;
        Free(reinterpret_cast<char*>(alloc_data) + 4, from.file, from.line, from.func);
    }
    
    if (!alloced_data_.empty())
    {
        LOG_ERR("ZPOOL ERROR, space not clear after PurgeMemory()");
    }
    
    for (int i=0; i<max_array_size; ++i)
    {
        if (pools_[i])
        {
            pools_[i]->purge_memory();
        }
    }
}

void GlobalMemPool::AddStats( const AllocFrom& alloced, int alloced_size )
{
    auto it = alloced_stats_data_.find(alloced);
    if (it == alloced_stats_data_.end())
    {
        auto ret = alloced_stats_data_.insert(std::make_pair(alloced, std::make_pair(alloced_size, alloced_size)));
        if (!ret.second)
        {
            return;
        }
        return;
    }
    else
    {
        auto& stats_data = it->second;
        stats_data.first += alloced_size;
        if (stats_data.second < stats_data.first)
            stats_data.second = stats_data.first;
    }
}

std::string GlobalMemPool::PrintStats()
{
    std::stringstream ss;
    uint64 total_memory = 0;
    for (auto it = alloced_stats_data_.begin(); it!=alloced_stats_data_.end(); ++it)
    {
        if (it->second.first == 0)
            continue;
        const AllocFrom& from = it->first;
        auto& stats_data = it->second;
        auto alloc_bytes =  stats_data.first;
        auto max_bytes = stats_data.second;
        total_memory += alloc_bytes;
        //LOG_ERR(boost::str(boost::format("memory leak: %s %d %s(): %d Bytes not freed.") % from.file % from.line % from.func % lost_bytes).c_str());
        ss << from.file << ":" << from.line << " " << from.func 
            << " alloc " << alloc_bytes 
            << " max alloc " << max_bytes 
            << std::endl;
    }
    ss << "total alloc memory :" << total_memory << std::endl;
    return ss.str();
}





}
}