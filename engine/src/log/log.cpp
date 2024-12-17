#include "global/platform.hpp"
#include "pool/object_pool.hpp"
#include "system/static_global.hpp"
#include "log.hpp"

/* 是否后台运行。后台运行则不输出日志到stdout */
static bool deamon_mode           = false;

// 设置一个默认日志文件名
// 防止上层应用来不及设置日志参数就发生错误，默认输出到工作目录error文件
static char printf_path[PATH_MAX] = "error";
static char error_path[PATH_MAX] = "error";

// 多线程中，可能某个线程写了日志就退出了，数据存用thread_local会被立马释放掉，只能存指针
// thread_local char prefix_name[LEN_PREFIX_NAME] = {0};
thread_local const char *prefix_name = nullptr;

thread_local int32_t buffer_len;
thread_local char buffer[20480];
#define FORMAT()                                                   \
    do                                                             \
    {                                                              \
        va_list args;                                              \
        va_start(args, fmt);                                       \
        buffer_len = vsnprintf(buffer, sizeof(buffer), fmt, args); \
        va_end(args);                                              \
        assert(buffer_len >= 0);                                   \
        if (buffer_len > (int32_t)sizeof(buffer))                  \
            buffer_len = (int32_t)sizeof(buffer) - 1;              \
    } while (0)

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

const char *get_error_path()
{
    return error_path;
}

const char *get_printf_path()
{
    return printf_path;
}

void set_prefix_name(const char *name)
{
    // 某个线程写异步日志，退出后释放了thread_local
    // 因此把数据存全局，保证日志线程写日志时还能获取到名字

    // null表示清空
    if (!name)
    {
        prefix_name = nullptr;
        return;
    }
    prefix_name = StaticGlobal::P->address(name);
}

const char* get_prefix_name()
{
    return prefix_name;
}

size_t write_prefix(FILE *stream, const char *prefix, const char *type,
                    int64_t time)
{
    // [T0LP12-17 16:15:53] T0即prefix表示worker，LP即type表示lua print
    fwrite("[", 1, 1, stream);
    if (prefix) fwrite(prefix, 1, strlen(prefix), stream);
    fwrite(type, 1, strlen(type), stream);

    thread_local char time_str[128] = {0};
    thread_local int64_t time_cache = 0;
    thread_local int32_t time_len   = 0;

    // 时间戳精度是1秒，同一秒写入的日志，直接取缓存。从日志结果看，这个缓存命中很高
    if (time != time_cache)
    {
        struct tm ntm;
        ::localtime_r(&time, &ntm);
        time_len = snprintf(time_str, sizeof(time_str),
                            "%02d-%02d %02d:%02d:%02d", ntm.tm_mon + 1,
                            ntm.tm_mday, ntm.tm_hour, ntm.tm_min, ntm.tm_sec);

        time_cache = time;
    }

    fwrite(time_str, 1, time_len, stream);
    return fwrite("]", 1, 1, stream);
}

static void tup_stream(const char *path, FILE *std_stream, time_t tm,
                       const char *type, const char *content)
{
    if (!deamon_mode)
    {
        write_prefix(std_stream, prefix_name, type, tm);
        fputs(content, std_stream);
        fputc('\n', std_stream);
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
    write_prefix(stream, prefix_name, type, tm);
    fputs(content, stream);
    fputc('\n', stream);
    ::fclose(stream);
}

void __sync_log(const char *path, FILE *stream, const char *prefix,
                const char *fmt, ...)
{
    FORMAT();
    tup_stream(path, stream, time(nullptr), prefix, buffer);
}

void __async_log(const char *path, LogType type, const char *fmt, ...)
{
    FORMAT();
    StaticGlobal::LOG->append(path, type, StaticGlobal::ev()->now(),
                                         buffer, buffer_len);
}
} // namespace log_util
