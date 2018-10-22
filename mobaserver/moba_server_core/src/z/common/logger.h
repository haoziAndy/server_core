#ifndef Z_COMMON_LOGGER_H
#define Z_COMMON_LOGGER_H

namespace z {
namespace common {


#define  FATAL_S "Fatal"
#define  ERROR_S "Error"
#define  WARN_S "Warn"
#define  INFO_S "Info"
#define  DEBUG_S "Debug"
/// thread-safe
class Logger
{
public:
    ~Logger()
    {
    }

    int Init(const std::string &server_name, const std::string &log_path, const int32 log_level);
    void Destroy();

private:

    Logger();

    DECLARE_SINGLETON(Logger);
};



} //namespace common
} //namespace z

#ifndef __PRETTY_FUNCTION__
#define __PRETTY_FUNCTION__ __FUNCTION__
#endif // __PRETTY_FUNCTION__

#ifndef __BASE_FILE__ 
#define __BASE_FILE__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#endif // __BASE_FILE__

#define LOGGER Singleton<z::common::Logger>::instance()

#define LOG4CPLUS(lv) log4cplus::Logger::getInstance(lv)

#define LOG_FATAL(format, ...)  LOG4CPLUS_FATAL_FMT(LOG4CPLUS(FATAL_S),format, ##__VA_ARGS__)
#define LOG_ERR(format, ...) LOG4CPLUS_ERROR_FMT(LOG4CPLUS(ERROR_S), format, ##__VA_ARGS__)
#define LOG_WARN(format, ...) LOG4CPLUS_WARN_FMT( LOG4CPLUS(WARN_S),format, ##__VA_ARGS__)
#define LOG_INFO(format, ...) LOG4CPLUS_INFO_FMT( LOG4CPLUS(INFO_S),format, ##__VA_ARGS__)
#define LOG_DEBUG(format, ...) LOG4CPLUS_DEBUG_FMT( LOG4CPLUS(DEBUG_S),format, ##__VA_ARGS__)


#endif //Z_COMMON_LOGGER_H
