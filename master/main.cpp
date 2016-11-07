#include "global/global.h"
#include "net/buffer.h"
#include "lua/lclass.h"
#include "lua/lstate.h"
#include "lua/leventloop.h"
#include "mysql/sql.h"
#include "mongo/mongo.h"
#include <sys/utsname.h> /* for uname */
#include "lua/lobj_counter.h"

int32 main( int32 argc,char **argv )
{
    if (argc < 4)
    {
        ERROR( "usage: [name] [index] [srvid]\n" );
        exit( 1 );
    }

    atexit(onexit);
    std::set_new_handler( new_fail );

    sql::library_init();
    mongo::init();

    class lstate *_lstate = lstate::instance();
    lua_State *L = _lstate->state();
    class leventloop *_loop = leventloop::instance();

    lclass<leventloop>::push( L,_loop,false );
    lua_setglobal( L,"ev" );

    /* 加载程序入口脚本 */
    char script_path[PATH_MAX];
    snprintf( script_path,PATH_MAX,"lua_script/%s/%s",argv[1],LUA_ENTERANCE );
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

    /* lua可能持有userdata，故要先关闭销毁 */
    _loop->finalize();               /* 优先解除lua_State依赖 */
    lstate::uninstance();            /* 最后关闭lua，其他模块引用太多lua_State */
    leventloop::uninstance();        /* 关闭主事件循环 */

    assert( "c++ object push to lua not release",
        obj_counter::instance()->final_check() );
    obj_counter::uninstance();       /* 关闭计数 */

    /* 清除静态数据，以免影响内存检测 */
    buffer::allocator.purge();
    sql::library_end();
    mongo::cleanup();

    return 0;
}
