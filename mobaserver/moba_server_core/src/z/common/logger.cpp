#include "stdafx.h"
#include "error.h"
#include "logger.h"
#include <climits>

namespace z {
namespace common {


enum
{
	log_buffer_size = 8192, 
	rotating_size = 1048576 * 10,//10M
};

Logger::Logger() {

}


int Logger::Init( const std::string &server_name, const std::string &log_path, const int32 log_level)
{
	const std::string file_name = log_path + "/" + server_name;

	log4cplus::tstring pattern = LOG4CPLUS_TEXT("%D{%m/%d/%y  %H:%M:%S}  [%l] - %m%n");
	log4cplus::SharedAppenderPtr  Console(new log4cplus::ConsoleAppender());

	log4cplus::SharedAppenderPtr TraceAppender(new log4cplus::RollingFileAppender(LOG4CPLUS_TEXT(file_name + ".trace.log"), rotating_size, SHRT_MAX, false, true));
	TraceAppender->setLayout(std::unique_ptr<log4cplus::Layout>(new log4cplus::PatternLayout(pattern)));
	log4cplus::Logger TraceLogger = log4cplus::Logger::getInstance(TRACE_S);
	TraceLogger.addAppender(Console);
	TraceLogger.addAppender(TraceAppender);
	TraceLogger.setLogLevel(log_level);

	log4cplus::SharedAppenderPtr DebugAppender(new log4cplus::RollingFileAppender(LOG4CPLUS_TEXT(file_name + ".debug.log"), rotating_size, SHRT_MAX, true,true));
	DebugAppender->setLayout(std::unique_ptr<log4cplus::Layout>(new log4cplus::PatternLayout(pattern)));
	log4cplus::Logger DebugLogger = log4cplus::Logger::getInstance(DEBUG_S);
	DebugLogger.addAppender(Console);
	DebugLogger.addAppender(DebugAppender);
	DebugLogger.setLogLevel(log_level);

	log4cplus::SharedAppenderPtr InfoAppender(new log4cplus::RollingFileAppender(LOG4CPLUS_TEXT(file_name + ".info.log"), rotating_size, SHRT_MAX, true, true));
	InfoAppender->setLayout(std::unique_ptr<log4cplus::Layout>(new log4cplus::PatternLayout(pattern)));
	log4cplus::Logger InfoLogger = log4cplus::Logger::getInstance(INFO_S);
	InfoLogger.addAppender(Console);
	InfoLogger.addAppender(InfoAppender);
	InfoLogger.setLogLevel(log_level);

	log4cplus::SharedAppenderPtr WarnAppender(new log4cplus::RollingFileAppender(LOG4CPLUS_TEXT(file_name + ".warn.log"), rotating_size, SHRT_MAX, true, true));
	WarnAppender->setLayout(std::unique_ptr<log4cplus::Layout>(new log4cplus::PatternLayout(pattern)));
	log4cplus::Logger WarnLogger = log4cplus::Logger::getInstance(WARN_S);
	WarnLogger.addAppender(Console);
	WarnLogger.addAppender(WarnAppender);
	WarnLogger.setLogLevel(log_level);

	log4cplus::SharedAppenderPtr ErrorAppender(new log4cplus::RollingFileAppender(LOG4CPLUS_TEXT(file_name + ".error.log"), rotating_size, SHRT_MAX, true, true));
	ErrorAppender->setLayout(std::unique_ptr<log4cplus::Layout>(new log4cplus::PatternLayout(pattern)));
	log4cplus::Logger ErrorLogger = log4cplus::Logger::getInstance(ERROR_S);
	ErrorLogger.addAppender(Console);
	ErrorLogger.addAppender(ErrorAppender);
	ErrorLogger.setLogLevel(log_level);

	log4cplus::SharedAppenderPtr FatalAppender(new log4cplus::RollingFileAppender(LOG4CPLUS_TEXT(file_name + ".fatal.log"), rotating_size, SHRT_MAX, true, true));
	FatalAppender->setLayout(std::unique_ptr<log4cplus::Layout>(new log4cplus::PatternLayout(pattern)));
	log4cplus::Logger FatalLogger = log4cplus::Logger::getInstance(FATAL_S);
	FatalLogger.addAppender(Console);
	FatalLogger.addAppender(FatalAppender);
	FatalLogger.setLogLevel(log_level);
	const log4cplus::LogLevelManager& llm = log4cplus::getLogLevelManager();
	LOG_INFO("Success Init Logger, LogLv = %s", llm.toString(log_level).c_str());
    return 0;
}



void Logger::Destroy()
{
}

} //namespace common
} //namespace z

