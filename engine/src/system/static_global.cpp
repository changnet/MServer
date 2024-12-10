#include "static_global.hpp"

#include "lua_cpplib/llib.hpp"
#include "mongo/mongo.hpp"
#include "mysql/sql.hpp"
#include "net/io/tls_ctx.hpp"
#include "net/socket.hpp"
#include "net/codec/pbc_codec.hpp"

class LEV *StaticGlobal::ev_                  = nullptr;
Buffer::ChunkPool *StaticGlobal::buffer_chunk_pool_ = nullptr;

// initializer最高等级初始化，在main函数之前，适合设置一些全局锁等
class StaticGlobal::initializer StaticGlobal::initializer_;

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

    TlsCtx::library_init();
    Sql::library_init();
    Mongo::init();
    Socket::library_init();
}

StaticGlobal::initializer::~initializer()
{
    Sql::library_end();
    Mongo::cleanup();
    TlsCtx::library_end();
    Socket::library_end();
}

void StaticGlobal::initialize()
{
    /**
     * 业务都放这里逻辑初始化
     * 原本用static对象，但实在是无法控制各个对象销毁的顺序。因为其他文件中还有局部static对象
     * 会依赖这些对象。
     * 比如class lev销毁时会释放引起一些对象gc，调用局部内存池，但早就销毁了
     * 各个变量之间有依赖，要注意顺序
     * 同一个unit中的static对象，先声明的先初始化。销毁的时候则反过来
     * 在头文件中的顺序不重要，这里实现的顺序才是运行时的顺序
     */

    // 先创建日志线程，保证其他模块能使用 ELOG 日志。如果在此之前需要日志用 ELOG_R
    E           = new class Env();
    LOG  = new class LLog("Engine.AsyncLog");
    M           = new class MainThread();
    ev_         = new class LEV();

    L                  = llib::new_state();
    buffer_chunk_pool_ = new Buffer::ChunkPool("buffer_chunk");

    LOG->set_thread_name("g_log");
    LOG->AsyncLog::start(1000);
}

void StaticGlobal::uninitialize()
{
    /* 业务逻辑都放这里销毁，不能放initializer的析构函数或者等到对应static对象析构
     * 因为业务逻辑非常复杂，他们之间相互引用。比如EV里包含io watcher，里面涉及
     * socket读写的buffer，buffer又涉及到内存池
     * 
     * 所以这里手动保证他们销毁的顺序。一些相互引用的，只能先调用他们的stop或者
     * clear函数来解除引用，再delete掉，不然valgrind会报Invalid read内存错误
     */

    L = llib::delete_state(L);

    // lua中销毁时，会gc socket，然后gc socket的buff，必须先在buffer_chunk_pool_前
    delete buffer_chunk_pool_;

    // 在最后面停止日志线程，保证其他模块写的日志还有效
    LOG->AsyncLog::stop();
    delete LOG;
    delete ev_;
    delete M;
    delete E;

    PbcCodec::uninitialize();
}

void on_exit()
{
    /* https://isocpp.org/files/papers/N3690.pdf 3.6.3 Termination
     * static变量创建和std::atexit的调用顺序相关，这里可能并没统计到
     * 所以全局变量和static变量统一放到static_global管理
     */

    int64_t counter  = 0;
    int64_t counters = 0;
    global_mem_counter(counter, counters);

    // 直接用PRINTF会导致重新创建ev取时间
    // PLOG( "new counter:%d    ----   new[] counter:%d",counter,counters );

    PLOG_R("new = " FMT64d " ----  new[] = " FMT64d, counter, counters);
    // back_trace();
}

void on_new_fail()
{
    FATAL("out of memory!!! abort");
}
