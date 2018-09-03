#ifndef Z_COMMON_LOGGER_H
#define Z_COMMON_LOGGER_H

namespace spdlog {
	class logger;
};

namespace z {
namespace common {

/// thread-safe
class Logger
{
public:
    ~Logger()
    {
        Destroy();
    }

    int Init(const std::string &server_name, const std::string &log_path, const int32 log_level);
    void Destroy();

	void Logger::Log(const char* format, ...);

    void Flush();

    char* datetime_str() {return datetime_str_;}
private:
	std::shared_ptr<spdlog::logger> file_logger_;

private:

	char datetime_str_[32];
	char* log_buffer_;
	int32 used_buffer_size_;

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

#define LOG_DEBUG_(format, ...) \
    LOGGER.Log("%s %s %s %d %s %" PRIu64 " %" PRIu64 " %d:" format, LOGGER.datetime_str(), "debug", \
    __FILE__, __LINE__, __PRETTY_FUNCTION__, GET_MSG_PLAYER(), GET_MSG_SEQ(), GET_MSG(), ##__VA_ARGS__)

#define LOG_FATAL(format, ...) LOG_DEBUG_(format, ##__VA_ARGS__)
#define LOG_ERR(format, ...) LOG_DEBUG_( format, ##__VA_ARGS__)
#define LOG_WARN(format, ...) LOG_DEBUG_( format, ##__VA_ARGS__)
#define LOG_INFO(format, ...) LOG_DEBUG_(format, ##__VA_ARGS__)
#ifndef NDEBUG
#define LOG_DEBUG(format, ...) LOG_DEBUG_(format, ##__VA_ARGS__)
#else
#define LOG_DEBUG(format, ...) 
#endif // NDEBUG


#endif //Z_COMMON_LOGGER_H
