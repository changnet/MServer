#include <lua.hpp>

#include "llog.h"
#include "../system/static_global.h"

llog::llog( lua_State *L )
{
    _log = NULL;
}

llog::~llog()
{
    if ( _log )
    {
        delete _log;
        _log = NULL;
    }
}

int32 llog::stop ( lua_State *L )
{
    if ( !_log || !_log->active() )
    {
        ERROR( "try to stop a inactive log thread" );
        return 0;
    }

    _log->stop();

    return 0;
}

int32 llog::start( lua_State *L )
{
    if ( _log )
    {
        luaL_error( L,"log thread already active" );
        return 0;
    }

    /* 设定多少秒写入一次 */
    int32 sec  = luaL_optinteger( L,1,5 );
    int32 usec = luaL_optinteger( L,2,0 );

    _log = new async_log();
    _log->start( sec,usec );

    return 0;
}

int32 llog::write( lua_State *L )
{
    // 如果从lua开始了一个独立线程，那么就用该线程写。否则共用全局异步日志线程
    class async_log *tl = _log ? _log : static_global::async_logger();

    if ( !tl->active() )
    {
        return luaL_error( L,"log thread inactive" );
    }

    size_t len = 0;
    const char *path = luaL_checkstring( L,1 );
    const char *ctx  = luaL_checklstring( L,2,&len );
    int32 out_type   = luaL_optinteger( L,3,LO_FILE );

    if ( out_type < LO_FILE || out_type >= LO_MAX )
    {
        return luaL_error( L,"log output type error" );
    }

    tl->write( path,ctx,len,static_cast<log_out_t>(out_type) );

    return 0;
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
    const char *mpath = luaL_checkstring( L,4 );

    set_log_args( dm,ppath,epath,mpath );
    return 0;
}

// 设置日志参数
int32 llog::set_name( lua_State *L )
{
    const char *name = luaL_checkstring( L,1 );

    set_app_name( name );
    return 0;
}
