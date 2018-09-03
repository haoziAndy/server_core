#ifndef ZEN_SINGLETON_H
#define ZEN_SINGLETON_H

/// @brief 只支持无参数构造T，from boost::singleton 
template <typename T>
class Singleton
{
public:
    struct object_creator
    {
        object_creator()
        { 
            Singleton<T>::instance(); 
        }

        inline void do_nothing()const 
        {}
    };

    static object_creator create_object;

public:
    typedef T object_type;
    static object_type& instance()
    {
        static object_type obj;
        create_object.do_nothing();
        return obj;
    }
};

template <typename T>
typename Singleton<T>::object_creator Singleton<T>::create_object; 


#ifndef DISALLOW_COPY_AND_ASSIGN
#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
    TypeName(const TypeName&);             \
    void operator=(const TypeName&) 
#endif

#define DECLARE_SINGLETON(TypeName)        \
    friend class Singleton<TypeName>;    \
    DISALLOW_COPY_AND_ASSIGN(TypeName)

#define DECLARE_SINGLETON_CONSTRUCTER(TypeName)        \
    TypeName() {};\
    friend class Singleton<TypeName>;    \
    DISALLOW_COPY_AND_ASSIGN(TypeName)

#endif //ZEN_SINGLETON_H

