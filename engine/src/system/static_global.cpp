#include "static_global.hpp"

#include "../mongo/mongo.hpp"
#include "../mysql/sql.hpp"

class LEV *StaticGlobal::_ev                  = nullptr;
class LState *StaticGlobal::_state            = nullptr;
class SSLMgr *StaticGlobal::_ssl_mgr          = nullptr;
class CodecMgr *StaticGlobal::_codec_mgr      = nullptr;
class Statistic *StaticGlobal::_statistic     = nullptr;
class LLog *StaticGlobal::_async_log          = nullptr;
class ThreadMgr *StaticGlobal::_thread_mgr    = nullptr;
class LNetworkMgr *StaticGlobal::_network_mgr = nullptr;

// initializer最高等级初始化，在main函数之前，适合设置一些全局锁等
class StaticGlobal::initializer StaticGlobal::_initializer;

/* will be called while process exit */
void on_exit();
/* will be called while allocate memory failed with new */
void on_new_fail();

StaticGlobal::initializer::initializer()
{
    /* https://isocpp.org/files/papers/N3690.pdf 3.6.3 Termination
     *  If a call to std::atexit is sequenced before the completion of the
     * initialization of an object with static storage duration, the call to
     * the destructor for the object is sequenced before the call to the
     * function passed to std::atexit.
     * 尽早调用atexit，这样才能保证静态变量的析构函数在onexit之前调用
     */
    atexit(on_exit);

    std::set_new_handler(on_new_fail);

    SSLMgr::library_init();
    Sql::library_init();
    Mongo::init();
    Socket::library_init();
}

StaticGlobal::initializer::~initializer()
{
    Sql::library_end();
    Mongo::cleanup();
    SSLMgr::library_end();
    Socket::library_end();
}

// 业务都放这里逻辑初始化
void StaticGlobal::initialize() /* 程序运行时初始化 */
{
    /* 原本用static对象，但实在是无法控制各个销毁的顺序。因为其他文件中还有局部static对象
     * 会依赖这些对象。
     * 比如class lev销毁时会释放引起一些对象gc，调用局部内存池，但早就销毁了
     * 各个变量之间有依赖，要注意顺序
     * 同一个unit中的static对象，先声明的先初始化。销毁的时候则反过来
     * 在头文件中的顺序不重要，这里实现的顺序才是运行时的顺序
     */

    // 先创建日志线程，保证其他模块能使用 ELOG 日志。如果在此之前需要日志用 ELOG_R
    _thread_mgr = new class ThreadMgr();
    _async_log  = new class LLog(nullptr);
    _ev         = new class LEV();

    _statistic   = new class Statistic();
    _state       = new class LState();
    _codec_mgr   = new class CodecMgr();
    _ssl_mgr     = new class SSLMgr();
    _network_mgr = new class LNetworkMgr();

    _async_log->AsyncLog::start(1000000);

    // 关服的时候，不需要等待这个线程。之前有人在关服定时器上打异步日志，导致这个线程一直忙
    // 关不了服。stop的时候会处理所有日志
    _async_log->set_wait_busy(false);

    // 初始化流量统计，这个和时间有关，等 ev 初始化后才调用
    _statistic->reset_trafic();
}

/* 业务逻辑都放这里销毁，不能放initializer的析构函数或者等到对应static对象析构
 * 因为业务逻辑可能包含static对象，等到调用析构的时候已经晚了
 * 比如socket的buffer对象中使用局部static内存池。如果在_network_mgr的析构里才关闭socket
 * 局部static内存池早就销毁了，内存早已出错。日志线程也是同样的设计。
 */
void StaticGlobal::uninitialize() /* 程序结束时反初始化 */
{
    _thread_mgr->stop(_async_log);
    _network_mgr->clear();

    delete _network_mgr;
    delete _ssl_mgr;
    delete _codec_mgr;
    delete _state;
    delete _statistic;

    // 在最后面停止日志线程，保证其他模块写的日志还有效
    _async_log->AsyncLog::stop();
    delete _thread_mgr;
    delete _async_log;
    delete _ev;
}

/* https://isocpp.org/files/papers/N3690.pdf
 * 3.6.3 Termination
 * static 变量的销毁与
 * static变更创建和std::atexit的调用顺序相关，这里可能并没统计到
 */
void on_exit()
{
    int64_t counter  = 0;
    int64_t counters = 0;
    global_mem_counter(counter, counters);

    // 直接用PRINTF会导致重新创建ev取时间
    // PLOG( "new counter:%d    ----   new[] counter:%d",counter,counters );

    PRINTF_R("new = " FMT64d " ----  new[] = " FMT64d, counter, counters);
    // back_trace();
}

void on_new_fail()
{
    FATAL("out of memory!!! abort");
}
