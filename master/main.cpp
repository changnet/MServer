#include "global/global.h"
#include "net/buffer.h"
#include "lua/lclass.h"
#include "lua/lstate.h"
#include "lua/leventloop.h"
#include "mysql/sql.h"
#include "mongo/mongo.h"
#include <sys/utsname.h> /* for uname */

/* 记录进程启动信息 */
void runtime_start()
{
    struct utsname buf;
    if( uname(&buf) )
    {
        perror("uname");
        exit(1);
    }

    time_t rawtime;
    time( &rawtime );
    struct tm *ntm = localtime( &rawtime );
    RUNTIME( "process[%d] run as '%s %d' %lubit at %04d-%02d-%02d %02d:%02d:%02d",
        getpid(),spath,sid,8*sizeof(void *),(ntm->tm_year + 1900),(ntm->tm_mon + 1),
        ntm->tm_mday, ntm->tm_hour, ntm->tm_min,ntm->tm_sec);
    RUNTIME( "lua version:%s",LUA_VERSION_MAJOR "." LUA_VERSION_MINOR "." LUA_VERSION_RELEASE );
    RUNTIME( "linux:%s %s",buf.release,buf.version );
}

/* 记录进程关闭信息 */
void runtime_stop()
{
    time_t rawtime;
    time( &rawtime );
    struct tm *ntm = localtime( &rawtime );
    
    RUNTIME( "process[%d] '%s %d' stop at %04d-%02d-%02d %02d:%02d:%02d",
        getpid(),spath,sid,(ntm->tm_year + 1900),(ntm->tm_mon + 1),
        ntm->tm_mday, ntm->tm_hour, ntm->tm_min,ntm->tm_sec);
}

/* 解析参数，不想依赖readline故不使用getopt */
void parse_args( int32 argc,char **argv,char *spath,int32 *psid )
{
    if (argc < 3)
    {
        ERROR( "parse arguments fail,terminated ..." );
        exit( 1 );
    }

    strcpy( spath,argv[1] );
    *psid = atoi( argv[2] );
}


int32 main( int32 argc,char **argv )
{
    parse_args( argc,argv,spath,&sid );
    /* 得到绝对工作路径,getcwd无法获取自启动程序的正确工作路径 */
    if ( getcwd( cwd,PATH_MAX ) <= 0 )
    {
        ERROR( "get current working directory fail\n" );
        exit( 1 );
    }

    atexit(onexit);
    std::set_new_handler( new_fail );

    sql::library_init();
    mongo::init();

    runtime_start();

    class lstate *_lstate = lstate::instance();
    lua_State *L = _lstate->state();
    class leventloop *_loop = leventloop::instance();

    lclass<leventloop>::push( L,_loop,false );
    lua_setglobal( L,"ev" );
    
    /* 加载程序入口脚本 */
    char script_path[PATH_MAX];
    snprintf( script_path,PATH_MAX,"lua_script/%s/%s",spath,LUA_ENTERANCE );
    if ( luaL_dofile(L,script_path) )
    {
        const char *err_msg = lua_tostring(L,-1);
        ERROR( "load lua enterance file error:%s\n",err_msg );
    }

    /* lua可能持有userdata，故要先关闭销毁 */
    _loop->finalize();               /* 优先解除lua_State依赖 */
    lstate::uninstance();            /* 最后关闭lua，其他模块引用太多lua_State */
    leventloop::uninstance();        /* 关闭主事件循环 */
    
    /* 清除静态数据，以免影响内存检测 */
    buffer::allocator.purge();
    sql::library_end();
    mongo::cleanup();

    runtime_stop();
    return 0;
}
