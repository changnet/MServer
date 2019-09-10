#pragma once

#include "log.h"
#include "../thread/thread.h"

// 多线程日志
class thread_log : public thread
{
public:
    thread_log();
    ~thread_log();

    size_t busy_job( size_t *finished = NULL,size_t *unfinished = NULL );

    void raw_write( const char *path,log_out_t out,const char *fmt,... );
    void write(const char *path,const char *ctx,size_t len,log_out_t out_type);
    void raw_write(
        const char *path,log_out_t out,const char *fmt,va_list args );
private:
    // 线程相关，重写基类相关函数
    bool uninitialize();
    void routine( notify_t notify );
    void notification( notify_t notify ) {}

    bool initialize() { return true; }
private:
    class log _log;
};
