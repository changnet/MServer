#pragma once

#include "log.h"
#include "../thread/thread.h"

// 多线程日志
class AsyncLog : public Thread
{
public:
    AsyncLog();
    ~AsyncLog();

    size_t busy_job(size_t *finished = NULL, size_t *unfinished = NULL);

    void raw_write(const char *path, LogOut out, const char *fmt, ...);
    void write(const char *path, const char *ctx, size_t len, LogOut out_type);
    void raw_write(const char *path, LogOut out, const char *fmt, va_list args);

private:
    // 线程相关，重写基类相关函数
    bool uninitialize();
    void routine(NotifyType notify);
    void notification(NotifyType notify) {}

    bool initialize() { return true; }

private:
    class Log _log;
};
