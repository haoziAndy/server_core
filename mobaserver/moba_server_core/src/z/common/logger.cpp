#include "stdafx.h"
#include "error.h"
#include "logger.h"
#include "memory_pool.h"
#include "boost/filesystem.hpp"
#include "spdlog/spdlog.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"


namespace z {
namespace common {

#ifdef _WIN32
#define snprintf sprintf_s
#endif

enum
{
	log_buffer_size = 8192, 
	rotating_size = 1048576 * 5,//5M
};

Logger::Logger():
	log_buffer_(nullptr)
	, used_buffer_size_(0)
{
	log_buffer_ = static_cast<char*>(malloc(log_buffer_size));
	datetime_str_[0] = '\0';
	file_logger_ = nullptr;
}

int Logger::Init( const std::string &server_name, const std::string &log_path,const int32 log_level )
{
	auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_st>();
	if (!boost::filesystem::exists(log_path)) {
		boost::filesystem::create_directories(log_path);
	}
	const std::string file_name = log_path + server_name + ".txt";
	auto rotating_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(file_name, rotating_size,30);
	std::vector<spdlog::sink_ptr> sinks{ console_sink, rotating_sink };
	file_logger_ = std::make_shared<spdlog::logger>(server_name, sinks.begin(),sinks.end());
	file_logger_->flush();
	LOG_DEBUG("Success to run logger,server_name = %s,file = %s",server_name.c_str(), log_path.c_str());
	spdlog::register_logger(file_logger_);
    return 0;
}



void Logger::Destroy()
{
	file_logger_->info("Close logger");
	file_logger_->flush();
}


void Logger::Flush()
{
	if (used_buffer_size_ == 0)
		return;
	file_logger_->flush();
	used_buffer_size_ = 0;
}

void Logger::Log(const char* format, ...)
{
	int length = 0;
	va_list ap;
	va_start(ap, format);
#ifdef _WIN32
#pragma warning(disable: 4996)
#endif
	length = vsnprintf(0, 0, format, ap);
#ifdef _WIN32
#pragma warning(default: 4996)
#endif
	va_end(ap);

	if (length <= 0 || length >= 0xFFFF)
		return;
	int32 avail_size = log_buffer_size - 10 - used_buffer_size_;
	if (length > avail_size)
	{
		Flush();
	}

	if (length <= avail_size)
	{
		char* buff = log_buffer_ + used_buffer_size_ + 2;

		va_start(ap, format);
#ifdef _WIN32
#pragma warning(disable: 4996)
#endif
		length = vsnprintf(buff, length + 1, format, ap);
#ifdef _WIN32
#pragma warning(default: 4996)
#endif
		va_end(ap);

		if (buff[length - 1] != '\n')
		{
			buff[length] = '\n';
			buff[length + 1] = '\0';
			length += 1;
		}
		used_buffer_size_ += length + 2;
		*reinterpret_cast<unsigned short *>(buff - 2) = length;
		file_logger_->info(buff);
		Flush();
	}
	else
	{
		// data ³¬³ö buff size 
		char* o_buff = static_cast<char*>(ZPOOL_MALLOC(length + 10));
		char* buff = o_buff + 2;
		va_start(ap, format);
#ifdef _WIN32
#pragma warning(disable: 4996)
#endif
		length = vsnprintf(buff, length + 1, format, ap);
#ifdef _WIN32
#pragma warning(default: 4996)
#endif
		va_end(ap);

		if (buff[length - 1] != '\n')
		{
			buff[length] = '\n';
			buff[length + 1] = '\0';
			length += 1;
		}
		*reinterpret_cast<unsigned short *>(buff - 2) = length;
		file_logger_->info(o_buff);
		Flush();
		ZPOOL_FREE(reinterpret_cast<void*>(const_cast<char*>(o_buff)));
	}
}
} //namespace common
} //namespace z

