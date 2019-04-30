#include <sys/utsname.h> /* for uname */


#include "net/buffer.h"
#include "scene/grid_aoi.h"
#include "net/io/ssl_mgr.h"
#include "util/statistic.h"
#include "lua_cpplib/lclass.h"
#include "net/codec/codec_mgr.h"
#include "system/static_global.h"

int32 main( int32 argc,char **argv )
{
    if (argc < 4)
    {
        ERROR( "usage: [name] [index] [srvid]\n" );
        exit( 1 );
    }

    lua_State *L = static_global::state();

    lclass<lev>::push( L,static_global::lua_ev(),false );
    lua_setglobal( L,"ev" );

    lclass<lnetwork_mgr>::push( L,static_global::network_mgr(),false );
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

    codec_mgr::uninstance   ();      /* 销毁数据编码对象 */
    ssl_mgr::uninstance     ();      /* 销毁ssl上下文 */

    statistic::uninstance();       /* 关闭计数 */

    /* 清除静态数据，以免影响内存检测 */
    buffer::purge();
    grid_aoi::purge();

    return 0;
}

