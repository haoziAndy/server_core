#ifndef Z_COMMON_MEMORY_POOL_H
#define Z_COMMON_MEMORY_POOL_H


namespace z
{
namespace common
{

/// MEM POOL 
/// 作为object_pool 使用时候, 最多支持9个参数的构造函数
class GlobalMemPool
{
    struct AllocFrom
    {
        const char* file;
        int line;
        const char* func;
    };
    struct LessAllocFrom
    {
        bool operator()(const AllocFrom& f1, const AllocFrom& f2) const
        {
            //return strcmp(f1.func, f2.func) < 0 && f1.line < f2.line ;
            /// @note should enable string pooling, vs /GF
            return f1.func < f2.func && f1.line < f2.line ;
        }
    };
    struct HashAllocFrom
    {
        size_t operator()(const AllocFrom& k) const
        {
            intptr_t addr_int = reinterpret_cast<intptr_t>(k.file);
            return std::hash<intptr_t>()(addr_int) + std::hash<int32>()(k.line);
        }
    };
    struct EqualAllocFrom
    {
        bool operator()(const AllocFrom& f1, const AllocFrom& f2) const
        {
            return f1.func == f2.func && f1.line == f2.line ;
        }
    };

    typedef std::unordered_map<void*, AllocFrom> AllocFromData;
    // std::pair<cur_alloc, max_alloc>
    typedef std::unordered_map<AllocFrom, std::pair<int, int>, HashAllocFrom, EqualAllocFrom> AllocStatsData;

public:
    GlobalMemPool();

    ~GlobalMemPool();

    void* Malloc(const char* file, const int line, const char* func, int size);
    
    void Free(void* const data, const char* file, const int line, const char* func);
    
    // void Free(void* const data);

    void PurgeMemory();

    void* Malloc_r(const char* file, const int line, const char* func, int size)
    {
        boost::mutex::scoped_lock lock(mutex_);
        return Malloc(file, line, func, size);
    }

    void Free_r(void* const data, const char* file, const int line, const char* func)
    {
        boost::mutex::scoped_lock lock(mutex_);
        Free(data, file, line, func);
    }
    void PurgeMemory_r()
    {
        boost::mutex::scoped_lock lock(mutex_);
        PurgeMemory();
    }
    void set_need_stats(bool need)
    {
        need_stats_ = need;
    }
    // vs2012 dont support c++11 variadic templates 
    // until Visual C++ Compiler November 2012 CTP, so ...
    //     template <typename T, typename... Args>
    //     T* New(const char* file, const int line, const char* func, Args... args)
    //     {
    //         int size = sizeof(T);
    //         void* data = Malloc(file, line, func, size);
    //         if (data == nullptr)
    //             return nullptr;
    //         T* p = reinterpret_cast<T*>(data);
    //         new(p)T(args...);
    // 
    //         return p;
    //     }

    template <typename T>
    T* New(const char* file, const int line, const char* func)
    {
        int size = sizeof(T);
        void* data = Malloc(file, line, func, size);
        if (data == nullptr)
            return nullptr;
        T* p = reinterpret_cast<T*>(data);
        new(p)T();

        return p;
    }
    template <typename T, typename Arg1>
    T* New(const char* file, const int line, const char* func, Arg1 arg1)
    {
        int size = sizeof(T);
        void* data = Malloc(file, line, func, size);
        if (data == nullptr)
            return nullptr;
        T* p = reinterpret_cast<T*>(data);
        new(p)T(arg1);

        return p;
    }
    template <typename T, typename Arg1, typename Arg2>
    T* New(const char* file, const int line, const char* func, Arg1 arg1, Arg2 arg2)
    {
        int size = sizeof(T);
        void* data = Malloc(file, line, func, size);
        if (data == nullptr)
            return nullptr;
        T* p = reinterpret_cast<T*>(data);
        new(p)T(arg1, arg2);

        return p;
    }
    template <typename T, typename Arg1, typename Arg2, typename Arg3>
    T* New(const char* file, const int line, const char* func, Arg1 arg1, Arg2 arg2, Arg3 arg3)
    {
        int size = sizeof(T);
        void* data = Malloc(file, line, func, size);
        if (data == nullptr)
            return nullptr;
        T* p = reinterpret_cast<T*>(data);
        new(p)T(arg1, arg2, arg3);

        return p;
    }
    template <typename T, typename Arg1, typename Arg2, typename Arg3, typename Arg4>
    T* New(const char* file, const int line, const char* func, Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4)
    {
        int size = sizeof(T);
        void* data = Malloc(file, line, func, size);
        if (data == nullptr)
            return nullptr;
        T* p = reinterpret_cast<T*>(data);
        new(p)T(arg1, arg2, arg3, arg4);

        return p;
    }
    template <typename T, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5>
    T* New(const char* file, const int line, const char* func, 
        Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5)
    {
        int size = sizeof(T);
        void* data = Malloc(file, line, func, size);
        if (data == nullptr)
            return nullptr;
        T* p = reinterpret_cast<T*>(data);
        new(p)T(arg1, arg2, arg3, arg4, arg5);

        return p;
    }
    template <typename T, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6>
    T* New(const char* file, const int line, const char* func, 
        Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5, Arg6 arg6)
    {
        int size = sizeof(T);
        void* data = Malloc(file, line, func, size);
        if (data == nullptr)
            return nullptr;
        T* p = reinterpret_cast<T*>(data);
        new(p)T(arg1, arg2, arg3, arg4, arg5, arg6);

        return p;
    }
    template <typename T, typename Arg1, typename Arg2, typename Arg3, typename Arg4, 
        typename Arg5, typename Arg6, typename Arg7>
    T* New(const char* file, const int line, const char* func, 
        Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5, Arg6 arg6, Arg7 arg7)
    {
        int size = sizeof(T);
        void* data = Malloc(file, line, func, size);
        if (data == nullptr)
            return nullptr;
        T* p = reinterpret_cast<T*>(data);
        new(p)T(arg1, arg2, arg3, arg4, arg5, arg6, arg7);

        return p;
    }
    template <typename T, typename Arg1, typename Arg2, typename Arg3, typename Arg4, 
        typename Arg5, typename Arg6, typename Arg7, typename Arg8>
    T* New(const char* file, const int line, const char* func, 
        Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5, Arg6 arg6, Arg7 arg7, Arg8 arg8)
    {
        int size = sizeof(T);
        void* data = Malloc(file, line, func, size);
        if (data == nullptr)
            return nullptr;
        T* p = reinterpret_cast<T*>(data);
        new(p)T(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8);

        return p;
    }
    template <typename T, typename Arg1, typename Arg2, typename Arg3, typename Arg4, 
        typename Arg5, typename Arg6, typename Arg7, typename Arg8, typename Arg9>
        T* New(const char* file, const int line, const char* func, 
        Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5, Arg6 arg6, Arg7 arg7, Arg8 arg8, Arg9 arg9)
    {
        int size = sizeof(T);
        void* data = Malloc(file, line, func, size);
        if (data == nullptr)
            return nullptr;
        T* p = reinterpret_cast<T*>(data);
        new(p)T(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9);

        return p;
    }

    template <typename T>
    void Delete(T* object, const char* file, const int line, const char* func)
    {
        if (object == nullptr)
            return;

        object->~T();

        Free(object, file, line, func);        
    }

    template <typename T>
    T* New_r(const char* file, const int line, const char* func)
    {
        int size = sizeof(T);
        void* data = Malloc_r(file, line, func, size);
        if (data == nullptr)
            return nullptr;
        T* p = reinterpret_cast<T*>(data);
        new(p)T();

        return p;
    }
    template <typename T, typename Arg1>
    T* New_r(const char* file, const int line, const char* func, Arg1 arg1)
    {
        int size = sizeof(T);
        void* data = Malloc_r(file, line, func, size);
        if (data == nullptr)
            return nullptr;
        T* p = reinterpret_cast<T*>(data);
        new(p)T(arg1);

        return p;
    }
    template <typename T, typename Arg1, typename Arg2>
    T* New_r(const char* file, const int line, const char* func, Arg1 arg1, Arg2 arg2)
    {
        int size = sizeof(T);
        void* data = Malloc_r(file, line, func, size);
        if (data == nullptr)
            return nullptr;
        T* p = reinterpret_cast<T*>(data);
        new(p)T(arg1, arg2);

        return p;
    }
    template <typename T, typename Arg1, typename Arg2, typename Arg3>
    T* New_r(const char* file, const int line, const char* func, Arg1 arg1, Arg2 arg2, Arg3 arg3)
    {
        int size = sizeof(T);
        void* data = Malloc_r(file, line, func, size);
        if (data == nullptr)
            return nullptr;
        T* p = reinterpret_cast<T*>(data);
        new(p)T(arg1, arg2, arg3);

        return p;
    }
    template <typename T, typename Arg1, typename Arg2, typename Arg3, typename Arg4>
    T* New_r(const char* file, const int line, const char* func, Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4)
    {
        int size = sizeof(T);
        void* data = Malloc_r(file, line, func, size);
        if (data == nullptr)
            return nullptr;
        T* p = reinterpret_cast<T*>(data);
        new(p)T(arg1, arg2, arg3, arg4);

        return p;
    }
    template <typename T, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5>
    T* New_r(const char* file, const int line, const char* func, 
        Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5)
    {
        int size = sizeof(T);
        void* data = Malloc_r(file, line, func, size);
        if (data == nullptr)
            return nullptr;
        T* p = reinterpret_cast<T*>(data);
        new(p)T(arg1, arg2, arg3, arg4, arg5);

        return p;
    }
    template <typename T, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6>
    T* New_r(const char* file, const int line, const char* func, 
        Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5, Arg6 arg6)
    {
        int size = sizeof(T);
        void* data = Malloc_r(file, line, func, size);
        if (data == nullptr)
            return nullptr;
        T* p = reinterpret_cast<T*>(data);
        new(p)T(arg1, arg2, arg3, arg4, arg5, arg6);

        return p;
    }
    template <typename T, typename Arg1, typename Arg2, typename Arg3, typename Arg4, 
        typename Arg5, typename Arg6, typename Arg7>
        T* New_r(const char* file, const int line, const char* func, 
        Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5, Arg6 arg6, Arg7 arg7)
    {
        int size = sizeof(T);
        void* data = Malloc_r(file, line, func, size);
        if (data == nullptr)
            return nullptr;
        T* p = reinterpret_cast<T*>(data);
        new(p)T(arg1, arg2, arg3, arg4, arg5, arg6, arg7);

        return p;
    }
    template <typename T, typename Arg1, typename Arg2, typename Arg3, typename Arg4, 
        typename Arg5, typename Arg6, typename Arg7, typename Arg8>
        T* New_r(const char* file, const int line, const char* func, 
        Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5, Arg6 arg6, Arg7 arg7, Arg8 arg8)
    {
        int size = sizeof(T);
        void* data = Malloc_r(file, line, func, size);
        if (data == nullptr)
            return nullptr;
        T* p = reinterpret_cast<T*>(data);
        new(p)T(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8);

        return p;
    }
    template <typename T, typename Arg1, typename Arg2, typename Arg3, typename Arg4, 
        typename Arg5, typename Arg6, typename Arg7, typename Arg8, typename Arg9>
        T* New_r(const char* file, const int line, const char* func, 
        Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5, Arg6 arg6, Arg7 arg7, Arg8 arg8, Arg9 arg9)
    {
        int size = sizeof(T);
        void* data = Malloc_r(file, line, func, size);
        if (data == nullptr)
            return nullptr;
        T* p = reinterpret_cast<T*>(data);
        new(p)T(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9);

        return p;
    }

    template <typename T>
    void Delete_r(T* object, const char* file, const int line, const char* func)
    {
        if (object == nullptr)
            return;

        object->~T();

        Free_r(object, file, line, func);        
    }

    // shared_ptr
    template <typename T>
    boost::shared_ptr<T> NewShared(const char* file, const int line, const char* func)
    {
        int size = sizeof(T);
        void* data = Malloc(file, line, func, size);
        if (data == nullptr)
            return nullptr;
        T* p = reinterpret_cast<T*>(data);
        new(p)T();

        return boost::shared_ptr<T>(p, boost::bind(&GlobalMemPool::Delete<T>, this, _1, file, line, func));
    }
    template <typename T, typename Arg1>
    boost::shared_ptr<T> NewShared(const char* file, const int line, const char* func, Arg1 arg1)
    {
        int size = sizeof(T);
        void* data = Malloc(file, line, func, size);
        if (data == nullptr)
            return nullptr;
        T* p = reinterpret_cast<T*>(data);
        new(p)T(arg1);

        return boost::shared_ptr<T>(p, boost::bind(&GlobalMemPool::Delete<T>, this, _1, file, line, func));
    }
    template <typename T, typename Arg1, typename Arg2>
    boost::shared_ptr<T> NewShared(const char* file, const int line, const char* func, Arg1 arg1, Arg2 arg2)
    {
        int size = sizeof(T);
        void* data = Malloc(file, line, func, size);
        if (data == nullptr)
            return nullptr;
        T* p = reinterpret_cast<T*>(data);
        new(p)T(arg1, arg2);

        return boost::shared_ptr<T>(p, boost::bind(&GlobalMemPool::Delete<T>, this, _1, file, line, func));
    }
    template <typename T, typename Arg1, typename Arg2, typename Arg3>
    boost::shared_ptr<T> NewShared(const char* file, const int line, const char* func, Arg1 arg1, Arg2 arg2, Arg3 arg3)
    {
        int size = sizeof(T);
        void* data = Malloc(file, line, func, size);
        if (data == nullptr)
            return nullptr;
        T* p = reinterpret_cast<T*>(data);
        new(p)T(arg1, arg2, arg3);

        return boost::shared_ptr<T>(p, boost::bind(&GlobalMemPool::Delete<T>, this, _1, file, line, func));
    }
    template <typename T, typename Arg1, typename Arg2, typename Arg3, typename Arg4>
    boost::shared_ptr<T> NewShared(const char* file, const int line, const char* func, Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4)
    {
        int size = sizeof(T);
        void* data = Malloc(file, line, func, size);
        if (data == nullptr)
            return nullptr;
        T* p = reinterpret_cast<T*>(data);
        new(p)T(arg1, arg2, arg3, arg4);

        return boost::shared_ptr<T>(p, boost::bind(&GlobalMemPool::Delete<T>, this, _1, file, line, func));
    }
    template <typename T, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5>
    boost::shared_ptr<T> NewShared(const char* file, const int line, const char* func, 
        Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5)
    {
        int size = sizeof(T);
        void* data = Malloc(file, line, func, size);
        if (data == nullptr)
            return nullptr;
        T* p = reinterpret_cast<T*>(data);
        new(p)T(arg1, arg2, arg3, arg4, arg5);

        return boost::shared_ptr<T>(p, boost::bind(&GlobalMemPool::Delete<T>, this, _1, file, line, func));
    }
    template <typename T, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6>
    boost::shared_ptr<T> NewShared(const char* file, const int line, const char* func, 
        Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5, Arg6 arg6)
    {
        int size = sizeof(T);
        void* data = Malloc(file, line, func, size);
        if (data == nullptr)
            return nullptr;
        T* p = reinterpret_cast<T*>(data);
        new(p)T(arg1, arg2, arg3, arg4, arg5, arg6);

        return boost::shared_ptr<T>(p, boost::bind(&GlobalMemPool::Delete<T>, this, _1, file, line, func));
    }
    template <typename T, typename Arg1, typename Arg2, typename Arg3, typename Arg4, 
        typename Arg5, typename Arg6, typename Arg7>
    boost::shared_ptr<T> NewShared(const char* file, const int line, const char* func, 
        Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5, Arg6 arg6, Arg7 arg7)
    {
        int size = sizeof(T);
        void* data = Malloc(file, line, func, size);
        if (data == nullptr)
            return nullptr;
        T* p = reinterpret_cast<T*>(data);
        new(p)T(arg1, arg2, arg3, arg4, arg5, arg6, arg7);

        return boost::shared_ptr<T>(p, boost::bind(&GlobalMemPool::Delete<T>, this, _1, file, line, func));
    }
    template <typename T, typename Arg1, typename Arg2, typename Arg3, typename Arg4, 
        typename Arg5, typename Arg6, typename Arg7, typename Arg8>
    boost::shared_ptr<T> NewShared(const char* file, const int line, const char* func, 
        Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5, Arg6 arg6, Arg7 arg7, Arg8 arg8)
    {
        int size = sizeof(T);
        void* data = Malloc(file, line, func, size);
        if (data == nullptr)
            return nullptr;
        T* p = reinterpret_cast<T*>(data);
        new(p)T(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8);

        return boost::shared_ptr<T>(p, boost::bind(&GlobalMemPool::Delete<T>, this, _1, file, line, func));
    }
    template <typename T, typename Arg1, typename Arg2, typename Arg3, typename Arg4, 
        typename Arg5, typename Arg6, typename Arg7, typename Arg8, typename Arg9>
    boost::shared_ptr<T> NewShared(const char* file, const int line, const char* func, 
        Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5, Arg6 arg6, Arg7 arg7, Arg8 arg8, Arg9 arg9)
    {
        int size = sizeof(T);
        void* data = Malloc(file, line, func, size);
        if (data == nullptr)
            return nullptr;
        T* p = reinterpret_cast<T*>(data);
        new(p)T(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9);

        return boost::shared_ptr<T>(p, boost::bind(&GlobalMemPool::Delete<T>, this, _1, file, line, func));
    }
    //
    std::string PrintStats();
    const AllocStatsData& stats_data() 
    {
        return alloced_stats_data_;
    }
private:

    void AddStats(const AllocFrom& alloced, int alloced_size);
    
    int GetArrayIndexBySize(int size);
    
    
private:
    AllocFromData alloced_data_;
    AllocStatsData alloced_stats_data_;

    /// 最小 alloc 16 B, 有效负载 8B 最大 16 << 16 = 1MB
    enum { start_alloc_size = 16};
    enum { max_array_size = 16};    

    boost::mutex mutex_;
    boost::pool<>* pools_[max_array_size];
    boost::thread::id thread_id_;
    // 是否需要统计
    bool need_stats_;
};


} //namespace common
} //namespace z

#define ZPOOL Singleton<z::common::GlobalMemPool>::instance()

#ifndef __PRETTY_FUNCTION__
#define __PRETTY_FUNCTION__ __FUNCTION__
#endif // __PRETTY_FUNCTION__

#ifndef USE_THREADS

#define ZPOOL_MALLOC(size) ZPOOL.Malloc(__FILE__, __LINE__, __PRETTY_FUNCTION__, size)
#define ZPOOL_FREE(data) ZPOOL.Free(data, __FILE__, __LINE__, __PRETTY_FUNCTION__)

#define ZPOOL_NEW(typename, ...) ZPOOL.New<typename>(__FILE__, __LINE__, __PRETTY_FUNCTION__, ##__VA_ARGS__)
#define ZPOOL_DELETE(t) ZPOOL.Delete(t, __FILE__, __LINE__, __PRETTY_FUNCTION__)

#define ZPOOL_NEW_SHARED(typename, ...) ZPOOL.NewShared<typename>(__FILE__, __LINE__, __PRETTY_FUNCTION__, ##__VA_ARGS__)

#define ZPOOL_DESTROY() //ZPOOL.PurgeMemory() // huang

#else

#define ZPOOL_MALLOC(size) ZPOOL.Malloc_r(__FILE__, __LINE__, __PRETTY_FUNCTION__, size)
#define ZPOOL_FREE(data) ZPOOL.Free_r(data, __FILE__, __LINE__, __PRETTY_FUNCTION__)

#define ZPOOL_NEW(typename, ...) ZPOOL.New_r<typename>(__FILE__, __LINE__, __PRETTY_FUNCTION__, ##__VA_ARGS__)
#define ZPOOL_DELETE(t) ZPOOL.Delete_r(t, __FILE__, __LINE__, __PRETTY_FUNCTION__)

#define ZPOOL_DESTROY() ZPOOL.PurgeMemory_r()

#endif // USE_THREADS

#endif //Z_COMMON_MEMORY_POOL_H
