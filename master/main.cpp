#include <openssl/ssl.h>
#include <sys/utsname.h> /* for uname */

#include "mysql/sql.h"
#include "net/buffer.h"
#include "mongo/mongo.h"
#include "net/io/ssl_mgr.h"
#include "net/codec/codec_mgr.h"
#include "lua_cpplib/lclass.h"
#include "lua_cpplib/lstate.h"
#include "lua_cpplib/leventloop.h"
#include "lua_cpplib/lobj_counter.h"
#include "lua_cpplib/lnetwork_mgr.h"

int32 ssl_init();
int32 ssl_uninit();

int32 main( int32 argc,char **argv )
{
    if (argc < 4)
    {
        ERROR( "usage: [name] [index] [srvid]\n" );
        exit( 1 );
    }

    atexit(onexit);
    std::set_new_handler( new_fail );

    ssl_init();
    sql::library_init();
    mongo::init();

    lua_State *L = lstate::instance()->state();

    class leventloop *loop = leventloop::instance();
    lclass<leventloop>::push( L,loop,false );
    lua_setglobal( L,"ev" );

    class lnetwork_mgr *network_mgr = lnetwork_mgr::instance();
    lclass<lnetwork_mgr>::push( L,network_mgr,false );
    lua_setglobal( L,"network_mgr" );

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
    lnetwork_mgr::uninstance();      /* 关闭网络管理 */
    leventloop::uninstance  ();      /* 关闭主事件循环 */
    codec_mgr::uninstance   ();      /* 销毁数据编码对象 */
    ssl_mgr::uninstance     ();      /* 销毁ssl上下文 */

    assert( "c++ object push to lua not release",
        obj_counter::instance()->final_check() );
    obj_counter::uninstance();       /* 关闭计数 */

    /* 清除静态数据，以免影响内存检测 */
    buffer::allocator.purge();
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