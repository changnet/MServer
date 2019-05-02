#include "thread_log.h"
#include "../system/static_global.h"

thread_log::thread_log() : thread("thread_log")
{
}

thread_log::~thread_log()
{
}

void thread_log::write(
    const char *path,const char *ctx,size_t len,log_out_t out_type)
{
    static class ev *ev = static_global::ev();

    /* 时间必须取主循环的帧，不能取即时的时间戳 */
    lock();
    _log.write_cache( ev->now(),path,ctx,len,out_type );
    unlock();
}

// 线程结束之前清理函数
bool thread_log::uninitialize()
{
    routine( NTF_NONE );
    return true;
}

// 线程主循环
void thread_log::routine( notify_t notify )
{
    UNUSED( notify );

    /* 把主线程缓存的数据交换到日志线程，尽量减少锁竞争 */
    lock();
    _log.swap();
    unlock();

    // 日志线程写入文件
    _log.flush();

    // 回收内存
    lock();
    _log.collect_mem();
    unlock();
}
