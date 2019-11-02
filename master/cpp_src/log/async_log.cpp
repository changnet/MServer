#include "async_log.h"
#include "../system/static_global.h"

AsyncLog::AsyncLog() : Thread("thread_log")
{
}

AsyncLog::~AsyncLog()
{
}

size_t AsyncLog::busy_job( size_t *finished,size_t *unfinished )
{
    lock();
    size_t unfinished_sz = _log.pending_size();

    if ( is_busy() ) unfinished_sz += 1;
    unlock();

    if ( finished ) *finished = 0;
    if ( unfinished ) *unfinished = unfinished_sz;

    return unfinished_sz;
}

void AsyncLog::write(
    const char *path,const char *ctx,size_t len,LogOut out_type)
{
    static class Ev *ev = StaticGlobal::ev();

    /* 时间必须取主循环的帧，不能取即时的时间戳 */
    lock();
    _log.write_cache( ev->now(),path,ctx,len,out_type );
    unlock();
}

void AsyncLog::raw_write(
    const char *path,LogOut out,const char *fmt,va_list args )
{
    static char ctx_buff[LOG_MAX_LENGTH];

    int32_t len = vsnprintf(ctx_buff,LOG_MAX_LENGTH,fmt,args);

    /* snprintf
     * 错误返回-1
     * 缓冲区不足，返回需要的缓冲区长度(即返回值>=缓冲区长度表示被截断)
     * 正确返回写入的字符串长度
     * 上面的返回值都不包含0结尾的长度
     */

    if ( len < 0 )
    {
        ERROR("raw_write snprintf fail");
        return;
    }

    write( path,ctx_buff,len > LOG_MAX_LENGTH ? LOG_MAX_LENGTH : len,out );
}
void AsyncLog::raw_write(
    const char *path,LogOut out,const char *fmt,... )
{
    va_list args;
    va_start(args,fmt);
    raw_write(path,out,fmt,args);
    va_end(args);
}

// 线程结束之前清理函数
bool AsyncLog::uninitialize()
{
    routine( NT_NONE );
    return true;
}

// 线程主循环
void AsyncLog::routine( NotifyType notify )
{
    UNUSED( notify );

    do
    {
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

        lock();
        _log.swap();
        unlock();
    } while ( !_log.empty() );

    _log.close_files();
}
