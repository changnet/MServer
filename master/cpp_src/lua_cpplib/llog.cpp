#include "llog.h"
#include "../global/clog.h"
#include "../lua_cpplib/leventloop.h"

llog::llog( lua_State *L )
{
}

llog::~llog()
{
}

int32 llog::stop ( lua_State *L )
{
    if ( !active() )
    {
        ERROR( "try to stop a inactive log thread" );
        return 0;
    }

    thread::stop();

    return 0;
}

int32 llog::start( lua_State *L )
{
    /* 设定多少秒写入一次 */
    int32 sec  = luaL_optinteger( L,1,5 );
    int32 usec = luaL_optinteger( L,2,0 );
    thread::start( sec,usec );

    return 0;
}

int32 llog::write( lua_State *L )
{
    if ( !active() )
    {
        return luaL_error( L,"log thread inactive" );
    }
    
    size_t len = 0;
    const char *path = luaL_checkstring( L,1 );
    const char *ctx  = luaL_checklstring( L,2,&len );

    static class leventloop *ev = leventloop::instance();

    /* 时间必须取主循环的帧，不能取即时的时间戳 */
    lock();
    _log.write_cache( ev->now(),path,ctx,len );
    unlock();

    return 0;
}

void llog::routine( notify_t msg )
{
    // none 是超时
    if ( NONE != msg ) return;

}

// 线程逻辑
void llog::do_routine()
{
    /* 把主线程缓存的数据交换到日志线程，尽量减少锁竞争 */
    lock();
    _log.swap();
    unlock();

    // 日志线程写入文件
    _log.flush();

    // 回收内存
    lock();
    _log.collect_mem();
    unlock();
}

bool llog::cleanup()
{
    /* 线程终止，所有日志入磁盘 */
    /* 不应该再有新日志进来，如果有，则丢日志 */
    do_routine();

    return true;
}

int32 llog::mkdir_p( lua_State *L )
{
    const char *path = luaL_checkstring( L,1 );
    if ( log::mkdir_p( path ) )
    {
        lua_pushboolean( L,1 );
    }
    else
    {
        lua_pushboolean( L,0 );
    }
    
    return 1;
}

// 用于实现stdout、文件双向输出日志打印函数
int32 llog::plog( lua_State *L )
{
    const char *ctx = luaL_checkstring( L,1 );
    // 这里要注意，不用%s，cprintf_log( "LP",ctx )这样直接调用也是可以的。但是如果脚本传
    // 入的字符串带特殊符号，如%，则可能会出错
    cprintf_log( "LP","%s",ctx );

    return 0;
}

// 用于实现stdout、文件双向输出日志打印函数
int32 llog::elog( lua_State *L )
{
    const char *ctx = luaL_checkstring( L,1 );
    cerror_log( "LE","%s",ctx );

    return 0;
}

// 设置日志参数
int32 llog::set_args( lua_State *L )
{
    bool dm = lua_toboolean( L,1 );
    const char *ppath = luaL_checkstring( L,2 );
    const char *epath = luaL_checkstring( L,3 );

    set_log_args( dm,ppath,epath );
    return 0;
}
