#ifndef __THREAD_H__
#define __THREAD_H__

#include <pthread.h>

#include "../ev/ev_watcher.h"
#include "../global/global.h"

class thread
{
public:
    virtual ~thread();
    explicit thread(const char *name);

    void stop ();
    bool start( int32 sec = 1,int32 usec = 0 );

    inline bool active() { return _run; }
    inline pthread_t get_id() { return _id; }
    inline const char *get_name() { return _name; }

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
    volatile bool _run;
    volatile bool _join;
    volatile bool _busy;
    pthread_mutex_t _mutex;

    const char *_name; // 线程名字，日志用而已
};

#endif /* __THREAD_H__ */
