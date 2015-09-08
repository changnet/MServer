#include "global/global.h"
#include "ev/ev.h"
#include "ev/ev_watcher.h"
#include <lua.hpp>
#include <sys/utsname.h> /* for uname */

class CTest
{
private:
    ev_io    m_io;
    ev_timer m_timer;
    
    ev_loop *loop;
public:
    explicit CTest( ev_loop *loop)
    : loop(loop)
    {
        
    }

    void io_cb(ev_io &w,int revents)
    {
        if ( EV_ERROR & revents )
        {
            std::cerr << "ev error" << std::endl;
            w.stop();
            return;
        }
        
        std::cout << "io_cb ... break" << std::endl;
        w.stop();
    }

    void timer_cb(ev_timer &w,int revents)
    {
        if ( EV_ERROR & revents ) // ev error
        {
            std::cerr << "ev error" << std::endl;
            w.stop();
            return;
        }

        ERROR("func this");
        DEBUG("try this");
        std::cout << "timer_cb ..." << std::endl;
    }

    void start()
    {
        m_io.set( loop );
        m_io.set<CTest,&CTest::io_cb>(this);
        m_io.start( 0,EV_READ );
        m_io.stop();
        m_io.start();
        
        m_timer.set(loop);
        m_timer.set<CTest,&CTest::timer_cb>(this);
        m_timer.start( 5 );
    }
};


void io_cb_ex(ev_io &w,int revents)
{
    if ( EV_ERROR & revents )
    {
        std::cerr << "ev error" << std::endl;
        w.stop();
        return;
    }
    
    std::cout << "io_cb_ex ... " << std::endl;
}

#include "lua_cpp/lclass.h"
    class lctest
    {
    public:
        lctest( lua_State *L){};
        void show()
        {
            std::cout << "show test" << std::endl;
        }
        
        static const char *classname;
    };
    
    const char *lctest::classname = "lctest";

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
    RUNTIME( "process[%d] run as '%s %d' %lubit at %04d-%02d-%02d %02d:%02d:%02d\n",
        getpid(),spath,sid,8*sizeof(void *),(ntm->tm_year + 1900),(ntm->tm_mon + 1), 
        ntm->tm_mday, ntm->tm_hour, ntm->tm_min,ntm->tm_sec);
    RUNTIME( "lua version:%s\n",LUA_VERSION_MAJOR "." LUA_VERSION_MINOR "." LUA_VERSION_RELEASE );
    RUNTIME( "linux:%s %s\n",buf.release,buf.version );
}

/* 记录进程关闭信息 */
void runtime_stop()
{
    time_t rawtime;
    time( &rawtime );
    struct tm *ntm = localtime( &rawtime );
    
    RUNTIME( "process[%d] '%s %d' stop at %04d-%02d-%02d %02d:%02d:%02d\n",
        getpid(),spath,sid,(ntm->tm_year + 1900),(ntm->tm_mon + 1), 
        ntm->tm_mday, ntm->tm_hour, ntm->tm_min,ntm->tm_sec);
}

/* 解析参数，不想依赖readline故不使用getopt */
void parse_args( int32 argc,char **argv,char *spath,int32 *psid )
{
    if (argc < 3)
    {
        ERROR( "parse arguments fail,terminated ...\n" );
        exit( 1 );
    }

    strcpy( spath,argv[1] );
    *psid = atoi( argv[2] );
}

/* 初始化lua环境 */
void lua_init( lua_State *L )
{
    /* 把当前工作目录加到lua的path */
    lua_getglobal(L, "package");
    lua_getfield(L, -1, "path");
    const char *old_path = lua_tostring(L, -1);

    char new_path[PATH_MAX] = {0};
    if ( snprintf( new_path,PATH_MAX,"%s;%s/?.lua",old_path,cwd ) >= PATH_MAX )
    {
        ERROR( "lua init,path overflow\n" );
        lua_close( L );
        exit( 1 );
    }

    lua_pop(L, 1);
    lua_pushstring(L, new_path);
    lua_setfield(L, -2, "path");
    lua_pop(L, 1);
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

    /* 初始化event loop */
    ev_loop loop;
    if ( !loop.init() )
    {
        ERROR( "ev loop init fail\n" );
        exit( 1 );
    }

    /* 初始化lua */
    lua_State* L = luaL_newstate();
    if ( !L )
    {
        ERROR( "lua new state fail\n" );
        exit( 1 );
    }
    luaL_openlibs(L);
    lua_init(L);

    runtime_start();

    //CTest t( &loop );
    //t.start();
    // ev_io io;
    // io.set( &loop );
    // io.set<io_cb_ex>();
    // io.start( 0,EV_WRITE );

    //loop.run();
    
    char script_path[PATH_MAX];
    snprintf( script_path,PATH_MAX,"lua/%s/%s",spath,LUA_ENTERANCE );
    if ( luaL_dofile(L,script_path) )
    {
        const char *err_msg = lua_tostring(L,-1);
        ERROR( "load lua enterance file error:%s\n",err_msg );
    }
        lclass<lctest> lc(L);
        if ( luaL_dofile(L,script_path) )
        {
            const char *err_msg = lua_tostring(L,-1);
            ERROR( "load lua enterance file error:%s\n",err_msg );
        }
    lua_gc(L, LUA_GCCOLLECT, 0);
    lua_close(L);

    runtime_stop();
    return 0;
}
