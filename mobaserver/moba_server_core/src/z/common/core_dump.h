
#ifndef Z_COMMON_CORE_DUMP_H_
#define Z_COMMON_CORE_DUMP_H_



namespace google_breakpad {class ExceptionHandler;};

namespace z {
namespace utils{


class CoreDumper
{
public:
    CoreDumper();
    ~CoreDumper();

    bool Init(const char* core_file_folder);

    void Destroy();

private:

    google_breakpad::ExceptionHandler* exception_handler_;


    DECLARE_SINGLETON(CoreDumper);
};



}; // namespace utils
}; // namespace z

#define CORE_DUMPER Singleton<z::utils::CoreDumper>::instance()

#endif // Z_COMMON_CORE_DUMP_H_

