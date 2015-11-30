#include "lsql.h"
#include "ltools.h"
#include "leventloop.h"
#include "../ev/ev_def.h"
#include "../net/socket.h"
#include "../thread/auto_mutex.h"

#define notify(s,x)                                            \
    do {                                                       \
        int8 val = static_cast<int8>(x);                       \
        int32 sz = ::write( s,&val,sizeof(int8) );             \
        if ( sz != sizeof(int8) )                              \
        {                                                      \
            ERROR( "lsql notify error:%s\n",strerror(errno) ); \
        }                                                      \
    }while(0)

#define invoke_cb( r )                                         \
    do {                                                       \
        if ( query->callback )                                 \
        {                                                      \
            sql_result result;                                 \
            result.id  = query->id;                            \
            result.err = _sql.get_errno();                     \
            result.res = r;                                    \
            pthread_mutex_lock( &mutex );                      \
            _result.push( result );                            \
            pthread_mutex_unlock( &mutex );                    \
            notify( fd[1],READ );                              \
        }                                                      \
        else assert( "sql should not have result",!r );        \
        delete query;                                          \
    }while(0)

lsql::lsql( lua_State *L )
    : L (L)
{
    fd[0] = -1;
    fd[1] = -1;
    
    ref_self  = 0;
    ref_read  = 0;
    ref_error = 0;
}

lsql::~lsql()
{
    assert( "sql thread not exit", !_run );

    if ( fd[0] > -1 ) ::close( fd[0] ); fd[0] = -1;
    if ( fd[1] > -1 ) ::close( fd[1] ); fd[1] = -1;
    
    LUA_UNREF( ref_self  );
    LUA_UNREF( ref_read  );
    LUA_UNREF( ref_error );
}

/* 连接mysql并启动线程 */
int32 lsql::start()
{
    if ( _run )
    {
        return luaL_error( L,"sql thread already active" );
    }

    const char *ip   = luaL_checkstring  ( L,1 );
    const int32 port = luaL_checkinteger ( L,2 );
    const char *usr  = luaL_checkstring  ( L,3 );
    const char *pwd  = luaL_checkstring  ( L,4 );
    const char *db   = luaL_checkstring  ( L,5 );

    _sql.set( ip,port,usr,pwd,db );
    /* fd[0]  父进程
     * fd[1]  子线程
     */
    if ( socketpair( AF_UNIX, SOCK_STREAM,IPPROTO_IP,fd ) < 0 )
    {
        FATAL( "socketpair fail:%s\n",strerror(errno) );
        return luaL_error( L,"socket pair fail" );
    }

    socket::non_block( fd[0] );    /* fd[1] need to be block */
    class ev_loop *loop = static_cast<ev_loop *>( leventloop::instance() );
    watcher.set( loop );
    watcher.set<lsql,&lsql::sql_cb>( this );
    watcher.start( fd[0],EV_READ );

    thread::start();
    return 0;
}

void lsql::routine()
{
    assert( "fd not valid",fd[1] > -1 );

    mysql_thread_init();
    if ( !_sql.connect() )
    {
        mysql_thread_end();
        notify( fd[1],ERR );
        return;
    }

    //--设置超时
    timeval tm;
    tm.tv_sec  = 5;
    tm.tv_usec = 0;
    setsockopt(fd[1], SOL_SOCKET, SO_RCVTIMEO, &tm.tv_sec,sizeof(struct timeval));
    setsockopt(fd[1], SOL_SOCKET, SO_SNDTIMEO, &tm.tv_sec, sizeof(struct timeval));

    int32 ets = 0;
    while ( _run )
    {
        /* 即使无查询，也定时ping服务器保持连接 */
        if ( _sql.ping() )
        {
            ++ets;
            if ( ets < 10 )
            {
                usleep(ets*1E6); // 1s
                continue;
            }

            ERROR( "mysql ping %d times still fail:%s\n",ets,_sql.error() );
            notify( fd[1],ERR );
            break;  // ping fail too many times,thread exit
        }

        ets = 0;
        int8 event = 0;
        int32 sz = ::read( fd[1],&event,sizeof(int8) ); /* 阻塞 */
        if ( sz < 0 )
        {
            /* errno variable is thread save */
            if ( errno == EAGAIN || errno == EWOULDBLOCK )
            {
                continue;  // just timeout
            }

            ERROR( "socketpair broken,sql thread exit" );
            // socket error,can't notify( fd[1],ERR );
            break;
        }
        
        switch ( event )
        {
            case EXIT : thread::stop();break;
            case ERR  : assert( "main thread should never nofity err",false );break;
            case READ : break;
        }

        invoke_sql(); /* 即使EXIT，也要将未完成的sql写入 */
    }
    
    _sql.disconnect() ;
    mysql_thread_end();
}

void lsql::invoke_sql()
{
    PDEBUG( "enter invoke_sql at:%ld",time(0) );
    clock_t t = clock();
    int32 cnt = 0;
    while ( true )
    {
        pthread_mutex_lock( &mutex );
        if ( _query.empty() )
        {
            pthread_mutex_unlock( &mutex );
            clock_t e = clock();
            PDEBUG( "query %d cost %f,leveing at %ld",cnt,float(e-t)/CLOCKS_PER_SEC,time(0) );
            return;
        }
        cnt ++;
        struct sql_query *query = _query.front();
        _query.pop();
        pthread_mutex_unlock( &mutex );

        const char *stmt = query->stmt;
        assert( "empty sql statement",stmt && query->size > 0 );
        
        if ( _sql.query ( stmt,query->size ) )
        {
            ERROR( "sql query error:%s\n",_sql.error() );
            ERROR( "sql not exec:%s\n",stmt );
            
            invoke_cb( NULL );
            continue;
        }

        struct sql_res *res = NULL;
        if ( _sql.result( &res ) )
        {
            ERROR( "sql result error[%s]:%s\n",stmt,_sql.error() );
            
            invoke_cb( res );
            continue;
        }
        
        invoke_cb( res );
    }
}

int32 lsql::stop()
{
    if ( !thread::_run )
    {
        return luaL_error( L,"try to stop a inactive sql thread" );
    }
    notify( fd[0],EXIT );

    return 0;
}

/* 此函数可能会阻塞 */
int32 lsql::join()
{
    thread::join();

    return 0;
}

int32 lsql::do_sql()
{
    size_t size = 0;
    int32 id = luaL_checkinteger( L,1 );
    const char *stmt = luaL_checklstring( L,2,&size );
    if ( !stmt || size <= 0 )
    {
        return luaL_error( L,"sql select,empty sql statement" );
    }
    
    int32 callback = lua_toboolean( L,3 );

    bool _notify = false;
    struct sql_query *query = new sql_query( id,callback,size,stmt );
    {
        class auto_mutex _auto_mutex( &mutex );
        if ( _query.empty() )  /* 不要使用_query.size() */
        {
            _notify = true;
        }
        _query.push( query );
    }

    /* 子线程的socket是阻塞的。主线程检测到子线程正常处理sql则无需告知。防止
     * 子线程socket缓冲区满造成Resource temporarily unavailable
     */
    if (_notify) notify( fd[0],READ );
    return 0;
}

void lsql::sql_cb( ev_io &w,int32 revents )
{
    int8 event = 0;
    int32 sz = ::read( w.fd,&event,sizeof(int8) );
    if ( sz < 0 )
    {
        if ( errno == EAGAIN || errno == EWOULDBLOCK )
        {
            assert( "non-block socket,should not happen",false );
            return;
        }
        
        ERROR( "sql socket error:%s\n",strerror(errno) );
        assert( "sql socket broken",false );
        return;
    }
    else if ( 0 == sz )
    {
        assert( "sql socketpair should not close",false);
        return;
    }
    else if ( sizeof(int8) != sz )
    {
        assert( "package incomplete,should not happen",false );
        return;
    }
    
    switch ( event )
    {
        case EXIT : assert( "sql thread should not exit itself",false );break;
        case ERR  : 
        {
            lua_rawgeti( L,LUA_REGISTRYINDEX,ref_error );
            int32 param = 0;
            if ( ref_self )
            {
                lua_rawgeti( L,LUA_REGISTRYINDEX,ref_self );
                param ++;
            }
            if ( LUA_OK != lua_pcall( L,param,0,0 ) )
            {
                ERROR( "sql error call back fail:%s\n",lua_tostring(L,-1) );
            }
        }break;
        case READ : 
        {
            lua_rawgeti( L,LUA_REGISTRYINDEX,ref_read );
            int32 param = 0;
            if ( ref_self )
            {
                lua_rawgeti( L,LUA_REGISTRYINDEX,ref_self );
                param ++;
            }
            if ( LUA_OK != lua_pcall( L,param,0,0 ) )
            {
                ERROR( "sql error call back fail:%s\n",lua_tostring(L,-1) );
            }
        }break;
        default   : assert( "unknow sql event",false );break;
    }
}

int32 lsql::read_callback()
{
    if ( !lua_isfunction(L,1) )
    {
        return luaL_error( L,"sql set read,argument #1 expect function" );
    }

    LUA_REF( ref_read );
    
    return 0;
}

int32 lsql::self_callback()
{
    if ( !lua_istable(L,1) )
    {
        return luaL_error( L,"sql set self,argument #1 expect table" );
    }

    LUA_REF( ref_self );
    
    return 0;
}

int32 lsql::error_callback()
{
    if ( !lua_isfunction(L,1) )
    {
        return luaL_error( L,"sql set error,argument #1 expect function" );
    }

    LUA_REF( ref_error );
    
    return 0;
}

int32 lsql::get_result()
{
    pthread_mutex_lock( &mutex );
    if ( _result.empty() )
    {
        pthread_mutex_unlock( &mutex );
        lua_pushnil( L );
        return 1;
    }
    
    struct sql_result r = _result.front();
    _result.pop();
    pthread_mutex_unlock( &mutex );

    lua_pushinteger( L,r.id );
    lua_pushinteger( L,r.err );
    if ( r.res )
    {
        result_encode( r.res );
        
        delete r.res;
        return 3;
    }
    
    return 2;
}

/* 将mysql结果集转换为lua table */
void lsql::result_encode( struct sql_res *res )
{
    assert( "sql result over boundary",res->num_cols == res->fields.size() &&
        res->num_rows == res->rows.size() );
    
    lua_createtable( L,res->num_rows,0 ); /* 创建数组，元素个数为num_rows */
    
    std::vector<sql_field> &fields = res->fields;
    std::vector<sql_row  > &rows   = res->rows;
    for ( uint32 row = 0;row < res->num_rows;row ++ )
    {
        lua_pushinteger( L,row + 1 ); /* lua table从1开始 */
        lua_createtable( L,0,res->num_cols ); /* 创建hash表，元素个数为num_cols */
        
        std::vector<sql_col> &cols = rows[row].cols;
        for ( uint32 col = 0;col < res->num_cols;col ++ )
        {
            assert( "sql result column over boundary",res->num_cols == cols.size() );
            
            if ( !cols[col].value ) continue;  /* 值为NULL */
            
            lua_pushstring( L,fields[col].name );
            switch ( fields[col].type )
            {
                case MYSQL_TYPE_TINY      :
                case MYSQL_TYPE_SHORT     :
                case MYSQL_TYPE_LONG      :
                case MYSQL_TYPE_TIMESTAMP :
                case MYSQL_TYPE_INT24     :
                    lua_pushinteger( L,static_cast<LUA_INTEGER>(atoi(cols[col].value)) );
                    break;
                case MYSQL_TYPE_LONGLONG  :
                    lua_pushinteger( L,static_cast<LUA_INTEGER>(atoll(cols[col].value)) );
                    break;
                case MYSQL_TYPE_FLOAT   :
                case MYSQL_TYPE_DOUBLE  :
                case MYSQL_TYPE_DECIMAL :
                    lua_pushnumber( L,static_cast<LUA_NUMBER>(atof(cols[col].value)) );
                    break;
                case MYSQL_TYPE_VARCHAR     :
                case MYSQL_TYPE_TINY_BLOB   :
                case MYSQL_TYPE_MEDIUM_BLOB :
                case MYSQL_TYPE_LONG_BLOB   :
                case MYSQL_TYPE_BLOB        :
                case MYSQL_TYPE_VAR_STRING  :
                case MYSQL_TYPE_STRING      :
                    lua_pushlstring( L,cols[col].value,cols[col].size );
                    break;
                default :
                    lua_pushnil( L );
                    ERROR( "unknow mysql type:%d\n",fields[col].type );
                    break;
            }
            
            lua_rawset( L, -3 );
        }
        
        lua_rawset( L, -3 );
    }
}
