#include <openssl/ssl.h>
#include "static_global.h"

#include "../mysql/sql.h"
#include "../mongo/mongo.h"

class static_global::initializer static_global::_initializer;
class lev static_global::_ev;
class lstate static_global::_state;
class thread_log static_global::_global_log;

int32 ssl_init();
int32 ssl_uninit();
/* will be called while process exit */
void on_exit();
/* will be called while allocate memory failed with new */
void on_new_fail();

static_global::initializer::initializer()
{
    /* https://isocpp.org/files/papers/N3690.pdf 3.6.3 Termination
     *  If a call to std::atexit is sequenced before the completion of the
     * initialization of an object with static storage duration, the call to
     * the destructor for the object is sequenced before the call to the
     * function passed to std::atexit.
     * 尽早调用atexit，这样才能保证静态变量的析构函数在onexit之前调用
     */
    atexit( on_exit );

    std::set_new_handler( on_new_fail );

    ssl_init();
    sql::library_init();
    mongo::init();
}

static_global::initializer::~initializer()
{
    sql::library_end();
    mongo::cleanup();
    ssl_uninit();
}


// 初始化ssl库
int32 ssl_init()
{
    /* sha1、base64等库需要用到的
     * mongo c driver、mysql c connector等第三方库可能已初始化了ssl
     * ssl初始化是不可重入的。在初始化期间不要再调用任何相关的ssl函数
     * ssl可以初始化多次，在openssl\crypto\init.c中通过RUN_ONCE来控制
     */

// OPENSSL_VERSION_NUMBER定义在/usr/include/openssl/opensslv.h
#if OPENSSL_VERSION_NUMBER < 0x10100000L
    SSL_library_init();
#else
    OPENSSL_init_ssl(0, NULL);
#endif

    SSL_load_error_strings ();
    ERR_load_BIO_strings ();
    OpenSSL_add_all_algorithms ();

    return 0;
}

int32 ssl_uninit()
{
    /* The OPENSSL_cleanup() function deinitialises OpenSSL (both libcrypto and
     * libssl). All resources allocated by OpenSSL are freed. Typically there
     * should be no need to call this function directly as it is initiated
     * automatically on application exit. This is done via the standard C
     * library atexit() function. In the event that the application will close
     * in a manner that will not call the registered atexit() handlers then the
     * application should call OPENSSL_cleanup() directly. Developers of
     * libraries using OpenSSL are discouraged from calling this function and
     * should instead, typically, rely on auto-deinitialisation. This is to
     * avoid error conditions where both an application and a library it depends
     * on both use OpenSSL, and the library deinitialises it before the
     * application has finished using it.
     */

    // OPENSSL_cleanup();

    return 0;
}

/* https://isocpp.org/files/papers/N3690.pdf
 * 3.6.3 Termination
 * static 变量的销毁与 static变更创建和std::atexit的调用顺序相关，这里可能并没统计到
 */
void on_exit()
{
    int32 counter  = 0;
    int32 counters = 0;
    global_mem_counter(counter,counters);

    // 直接用PRINTF会导致重新创建ev取时间
    // PRINTF( "new counter:%d    ----   new[] counter:%d",counter,counters );

    PRINTF_R( "new counter:%d    ----   new[] counter:%d",counter,counters );
    //back_trace();
}

void on_new_fail()
{
    FATAL( "out of memory!!! abort" );
}