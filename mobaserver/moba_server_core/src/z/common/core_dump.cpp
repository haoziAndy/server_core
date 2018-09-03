#include "stdafx.h"
#include "core_dump.h"
#include "logger.h"
#include "error.h"

#ifdef _WIN32
#include <google_breakpad/client/windows/handler/exception_handler.h>
#elif __linux__ 
#include <google_breakpad/client/linux/handler/exception_handler.h>
#endif

namespace z {
namespace utils{

CoreDumper::CoreDumper()
    : exception_handler_(nullptr)
{

}

CoreDumper::~CoreDumper()
{
    Destroy();
}

bool CoreDumper::Init( const char* core_file_folder )
{
    boost::system::error_code ec;
    if (!boost::filesystem::exists(core_file_folder, ec))
    {
        boost::filesystem::create_directories(core_file_folder, ec);
        if (!boost::filesystem::exists(core_file_folder, ec))
        {
            LOG_ERR("create_directories %s failed.", core_file_folder);
            return false;
        }
    }

#ifdef _WIN32
    wchar_t ws[1024];
    mbstowcs(ws, core_file_folder, sizeof(ws)/sizeof(ws[0]));

    exception_handler_ =
        new google_breakpad::ExceptionHandler(ws,
        nullptr,
        nullptr,
        nullptr,
        google_breakpad::ExceptionHandler::HANDLER_ALL);

    if (exception_handler_ == nullptr) 
    {
        LOG_ERR("failed create google_breakpad::ExceptionHandler");
        return false;
    }
#elif __linux__
    google_breakpad::MinidumpDescriptor descriptor(core_file_folder);
    exception_handler_ = 
        new google_breakpad::ExceptionHandler(descriptor,
        nullptr,
        nullptr,
        nullptr,
        true,
        -1);
#endif // _WIN32

    return true;
}

void CoreDumper::Destroy()
{
    if (exception_handler_ != nullptr)
    {
        delete exception_handler_;
        exception_handler_ = nullptr;
    }
}





}; // namespace utils
}; // namespace z

