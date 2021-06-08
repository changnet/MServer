#include "../pool/object_pool.hpp"
#include "../system/static_global.hpp"
#include "log.hpp"

/* 是否后台运行。后台运行则不输出日志到stdout */
static bool deamon_mode           = false;
static char printf_path[PATH_MAX] = "printf";
// 防止上层应用来不及设置日志参数就发生错误，默认输出到工作目录error文件
static char error_path[PATH_MAX] = "error";

// app进程名
static const int32_t LEN_APP_NAME  = 32;
static char app_name[LEN_APP_NAME] = {0};

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

/* 设置app进程名 */
void set_app_name(const char *name)
{
    snprintf(app_name, LEN_APP_NAME, "%s", name);
}

size_t write_prefix(FILE *stream, const char *prefix, int64_t time)
{
    thread_local char prefix_str[128]  = {0};
    thread_local char prefix_cache[32] = {0};
    thread_local int64_t time_cache = 0;
    thread_local int32_t prefix_len = 0;

    // 日志线程大约1秒输出一次日志，时间戳精度也是1秒。从日志结果看，这个缓存命中很高
    if (time != time_cache || 0 != strcmp(prefix, prefix_cache))
    {
        struct tm ntm;
        ::localtime_r(&time, &ntm);
        prefix_len = snprintf(prefix_str, sizeof(prefix_str),
                              "[%s%s%02d-%02d %02d:%02d:%02d]", app_name,
                              prefix, (ntm.tm_mon + 1), ntm.tm_mday,
                              ntm.tm_hour, ntm.tm_min, ntm.tm_sec);

        time_cache = time;
        snprintf(prefix_cache, sizeof(prefix_cache), "%s", prefix);
    }

    return fwrite(prefix_str, 1, prefix_len, stream);
}

static void tup_stream(const char *path, FILE *std_stream, time_t tm,
                       const char *prefix, const char *content)
{
    if (!deamon_mode)
    {
        write_prefix(std_stream, prefix, tm);
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
    write_prefix(stream, prefix, tm);
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
    StaticGlobal::async_logger()->append(path, type, StaticGlobal::ev()->now(),
                                         buffer, buffer_len);
}
