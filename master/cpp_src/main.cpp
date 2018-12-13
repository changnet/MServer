#include <openssl/ssl.h>
#include <sys/utsname.h> /* for uname */

#include "mysql/sql.h"
#include "net/buffer.h"
#include "mongo/mongo.h"
#include "scene/grid_aoi.h"
#include "net/io/ssl_mgr.h"
#include "net/codec/codec_mgr.h"
#include "lua_cpplib/lclass.h"
#include "lua_cpplib/lstate.h"
#include "lua_cpplib/lmap_mgr.h"
#include "lua_cpplib/lev.h"
#include "lua_cpplib/lobj_counter.h"
#include "lua_cpplib/lnetwork_mgr.h"

int32 ssl_init();
int32 ssl_uninit();
/* will be called while process exit */
void on_exit();
/* will be called while allocate memory failed with new */
void on_new_fail();

int32 main( int32 argc,char **argv )
{
    if (argc < 4)
    {
        ERROR( "usage: [name] [index] [srvid]\n" );
        exit( 1 );
    }

    /* https://isocpp.org/files/papers/N3690.pdf 3.6.3 Termination
     *  If a call to std::atexit is sequenced before the completion of the
     * initialization of an object with static storage duration, the call to
     * the destructor for the object is sequenced before the call to the
     * function passed to std::atexit.
     * 尽早调用atexit，这样才能保证静态变量的析构函数在onexit之前调用
     * 不过全局、静态变量初始化都比main早，且有
     * static initialization order fiasco(https://isocpp.org/wiki/faq/ctors)
     * 的问题，目前都是手动释放
     */
    atexit(on_exit);
    std::set_new_handler( on_new_fail );

    ssl_init();
    sql::library_init();
    mongo::init();

    lua_State *L = lstate::instance()->state();

    class lev *loop = lev::instance();
    lclass<lev>::push( L,loop,false );
    lua_setglobal( L,"ev" );

    class lnetwork_mgr *network_mgr = lnetwork_mgr::instance();
    lclass<lnetwork_mgr>::push( L,network_mgr,false );
    lua_setglobal( L,"network_mgr" );

    class lmap_mgr *map_mgr = lmap_mgr::instance();
    lclass<lmap_mgr>::push( L,map_mgr,false );
    lua_setglobal( L,"map_mgr" );

    /* 加载程序入口脚本 */
    char script_path[PATH_MAX];
    snprintf( script_path,PATH_MAX,"lua_src/%s",LUA_ENTERANCE );
    if ( LUA_OK != luaL_loadfile(L, script_path) )
    {
        const char *err_msg = lua_tostring(L,-1);
        ERROR( "load lua enterance file error:%s",err_msg );

        return 1;
    }

    /* push argv to lua */
    int cnt = 0;
    for ( int i = 0;i < argc && cnt < 8;i ++ )
    {
        lua_pushstring( L,argv[i] ); ++cnt;
    }

    if ( LUA_OK != lua_pcall(L, cnt, 0, 0) )
    {
        const char *err_msg = lua_tostring(L,-1);
        ERROR( "call lua enterance file error:%s",err_msg );

        return 1;
    }

    lstate::uninstance      ();      /* 关闭lua，其他模块引用太多lua_State */
    lmap_mgr::uninstance    ();      /* 关闭地图管理 */
    lnetwork_mgr::uninstance();      /* 关闭网络管理 */
    lev::uninstance  ();      /* 关闭主事件循环 */
    codec_mgr::uninstance   ();      /* 销毁数据编码对象 */
    ssl_mgr::uninstance     ();      /* 销毁ssl上下文 */

    assert( "c++ object push to lua not release",
        obj_counter::instance()->final_check() );
    obj_counter::uninstance();       /* 关闭计数 */

    /* 清除静态数据，以免影响内存检测 */
    buffer::purge();
    grid_aoi::purge();
    sql::library_end();
    mongo::cleanup();
    ssl_uninit();

    return 0;
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
    uint32 counter  = 0;
    uint32 counters = 0;
    global_mem_counter(counter,counters);
    PRINTF( "new counter:%d    ----   new[] counter:%d",counter,counters );
    //back_trace();
}

void on_new_fail()
{
    FATAL( "out of memory!!! abort" );
}
