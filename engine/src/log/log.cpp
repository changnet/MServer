#include "global/platform.hpp"
#include "pool/object_pool.hpp"
#include "system/static_global.hpp"
#include "log.hpp"
#include "ev/time.hpp"



namespace log_util
{
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
    // 输出到控制台
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
        write_stream("info.txt", stdout, str, len);
    }
    else
    {
        write_stream("error.txt", stdout, str, len);
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
