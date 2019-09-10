#pragma once

#include <atomic>
#include <pthread.h>

#include "../ev/ev_watcher.h"
#include "../global/global.h"

class thread
{
public:
    virtual ~thread();
    explicit thread(const char *name);

    /* 停止线程 */
    void stop ();
    /* 开始线程，可设置多长时间超时一次
     * @sec:秒
     * @usec:微秒
     */
    bool start( int32 sec = 1,int32 usec = 0 );
    /* 还在处理的数据
     * @finish:是子线程已处理完，等待主线程处理的数量
     * @unfinished:是等待子线程处理的数量
     * 返回总数
     */
    virtual size_t busy_job(
        size_t *finished = NULL,size_t *unfinished = NULL ) = 0;

    inline bool active() const { return _run; }
    /* 子线程是否正在处理数据，不在处理数据也不代表缓冲区没数据等待处理 */
    inline bool is_busy() const { return _busy;}
    inline pthread_t get_id() const { return _id; }
    inline const char *get_name() const { return _name; }

    inline bool is_wait_busy() const { return _wait_busy; }
    inline void set_wait_busy( bool busy ) { _wait_busy = busy; }

    static void signal_block();
public:
    typedef enum
    {
        NTF_ERROR  = -1, // 线程出现错误
        NTF_NONE   =  0, // 未定义的消息，比如有时候主线程只是想子线程运行一下routine，发起ping之类
        NTF_EXIT   =  1, // 通知子线程退出
        NTF_CUSTOM =  2, // 自定义消息，根据各线程自定义处理数据

        NTF_MAX
    }notify_t;
protected:
    virtual bool initialize() = 0;     /* 子线程初始化 */
    virtual bool uninitialize() = 0;    /* 子线程清理 */

    void notify_child( notify_t notify );     /* 通知子线程 */
    void notify_parent( notify_t notify );     /* 通知主线程 */

    virtual void routine( notify_t notify ) = 0;    /* 子线程通知处理 */
    virtual void notification( notify_t notify ) = 0;    /* 主线程收到通知 */

    static void *start_routine( void *arg );

    inline void lock() { pthread_mutex_lock( &_mutex ); }
    inline void unlock() { pthread_mutex_unlock( &_mutex ); }
private:
    void do_routine();
    void io_cb( ev_io &w,int32 revents );
private:
    int32 _fd[2]  ;
    ev_io _watcher;
    pthread_t _id;

    std::atomic<bool> _run;
    std::atomic<bool> _join;
    std::atomic<bool> _busy;
    pthread_mutex_t _mutex;

    bool _wait_busy; // 当关服的时候，是否需要等待这个线程
    const char *_name; // 线程名字，日志用而已
};
