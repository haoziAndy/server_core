#ifndef Z_NET_Z_CONTEXT_H
#define Z_NET_Z_CONTEXT_H


namespace z
{
namespace net
{

typedef void ZContext;
typedef void ZSocket;

class ZContextS
{
public:
    ~ZContextS()
    {
        DestroyZContext();
    }

    ZContext* context() { return context_;}
	int stop();

private:
    
    int InitZContext();

    int DestroyZContext();

private:
    ZContext* context_;


private:
    ZContextS() 
        : context_(NULL)
    {
        InitZContext();
    };
    DECLARE_SINGLETON(ZContextS);
};





}

}

#define ZCONTEXT Singleton<z::net::ZContextS>::instance()


#endif // Z_NET_Z_CONTEXT_H
