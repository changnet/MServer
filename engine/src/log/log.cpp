#include "global/platform.hpp"
#include "pool/object_pool.hpp"
#include "system/static_global.hpp"
#include "log.hpp"
#include "ev/time.hpp"

/* 是否后台运行。后台运行则不输出日志到stdout */
static bool deamon_mode           = false;

// 设置一个默认日志文件名
// 防止上层应用来不及设置日志参数就发生错误，默认输出到工作目录error文件
static char printf_path[PATH_MAX] = "error";
static char error_path[PATH_MAX] = "error";

// 多线程中，可能某个线程写了日志就退出了，数据存用thread_local会被立马释放掉
// 日志线程执行写入时，就取不到数据了，只能存指针
// thread_local char log_name[128] = {0};
thread_local const char *log_name = nullptr;

namespace log_util
{

/* 设置日志参数：是否后台，日志路径 */
void set_log_args(bool deamon, const char *ppath, const char *epath)
{
    deamon_mode = deamon;
    snprintf(printf_path, PATH_MAX, "%s", ppath);
    snprintf(error_path, PATH_MAX, "%s", epath);
}

bool is_deamon()
{
    return deamon_mode;
}

void set_log_name(const char *name)
{
    // 某个线程写异步日志，退出后释放了thread_local
    // 因此把数据存全局，保证日志线程写日志时还能获取到名字

    // null表示清空
    if (!name)
    {
        log_name = nullptr;
        return;
    }
    log_name = StaticGlobal::P->address(name);
}

const char* get_log_name()
{
    return log_name;
}

static void write_stream(const char *path, FILE *std_stream, const char *str,
                         int32_t len)
{
    // sync log是在日志线程未启动或者已关闭情况下紧急使用的，所以不考虑颜色之类花里胡哨的东西
    // 在日志线程未关闭的情况下，直接往文件或者控制台写会引起多线程导致显示不正确
    time_t t = time(nullptr);
    struct tm ntm;
    ::localtime_r(&t, &ntm);

    char time_str[128];
    int32_t time_len = snprintf(time_str, sizeof(time_str),
                                "[%02d-%02d %02d:%02d:%02d]", ntm.tm_mon + 1,
                                ntm.tm_mday, ntm.tm_hour, ntm.tm_min, ntm.tm_sec);
    if (!deamon_mode)
    {
        fwrite(time_str, 1, time_len, std_stream);
        fwrite(str, 1, len, std_stream);
        fwrite("\n", 1, 1, std_stream);
        fflush(std_stream);
    }

    FILE *stream = ::fopen(path, "ab+");
    if (!stream)
    {
        fputs("OPEN FILE FAIL:", stderr);
        fputs(strerror(errno), stderr);
        fputs(path, stderr);
        fputc('\n', stderr);
        return;
    }
    fwrite(time_str, 1, time_len, stream);
    fwrite(str, 1, len, stream);
    fwrite("\n", 1, 1, stream);
    ::fclose(stream);
}

void __sync_log(int32_t type, const char *str, int32_t len)
{
    if (1 == type)
    {
        write_stream(printf_path, stdout, str, len);
    }
    else
    {
        write_stream(printf_path, stdout, str, len);
    }
}

void __async_log(int32_t type, const char *str, int32_t len)
{
    if (1 == type)
    {
        StaticGlobal::LOG->AsyncLog::append("info", AsyncLog::MASK_S_C,
                                            timing::try_frame_time(), str, len);
    }
    else
    {
        StaticGlobal::LOG->AsyncLog::append(
            "error", AsyncLog::MASK_S_C | AsyncLog::MASK_C_R,
            timing::try_frame_time(), str, len);
    }
}

} // namespace log_util
