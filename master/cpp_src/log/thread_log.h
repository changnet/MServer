#ifndef __THREAD_LOG_H__
#define __THREAD_LOG_H__

#include "log.h"
#include "../thread/thread.h"

// 多线程日志
class thread_log : public thread
{
public:
    thread_log();
    ~thread_log();

    void write(const char *path,const char *ctx,size_t len,log_out_t out_type);
private:
    // 线程相关，重写基类相关函数
    bool cleanup();
    void routine( notify_t msg );
    void notification( notify_t msg ) {}

    bool initlization() { return true; }
private:
    class log _log;
};

#endif /* __THREAD_LOG_H__ */
