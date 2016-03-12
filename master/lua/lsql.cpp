#include "lsql.h"
#include "ltools.h"

lsql::lsql( lua_State *L )
    : L (L)
{
    ref_self  = 0;
    ref_read  = 0;
    ref_error = 0;
}

lsql::~lsql()
{
    LUA_UNREF( ref_self  );
    LUA_UNREF( ref_read  );
    LUA_UNREF( ref_error );
}

/* 连接mysql并启动线程 */
int32 lsql::start()
{
    if ( active() )
    {
        return luaL_error( L,"sql thread already active" );
    }

    const char *ip   = luaL_checkstring  ( L,1 );
    const int32 port = luaL_checkinteger ( L,2 );
    const char *usr  = luaL_checkstring  ( L,3 );
    const char *pwd  = luaL_checkstring  ( L,4 );
    const char *db   = luaL_checkstring  ( L,5 );

    _sql.set( ip,port,usr,pwd,db );

    thread::start( 10 );  /* 10s ping一下mysql */
    return 0;
}

void lsql::routine( notify_t msg )
{
    int32 ping = 0;
    int32 ets  = 0;
    /* 即使无查询，也定时ping服务器保持连接 */
    while ( ets < 10 && ( ping = _sql.ping() ) )
    {
        ++ets;
        usleep(ets*1E6); // ets*1s
    }

    if ( ping )
    {
        ERROR( "mysql ping %d times still fail:%s",ets,_sql.error() );
        notify_parent( ERROR );
        return;
    }
    
    switch ( msg )
    {
        case ERROR : assert( "main thread should never notify error",false );break;
        case NONE  : break; /* just timeout */
        case EXIT  : break; /* do nothing,auto exit */
        case MSG   : invoke_sql();break;
    }
}

void lsql::invoke_sql( bool cb )
{
    while ( true )
    {
        lock();
        if ( _query.empty() )
        {
            unlock();
            return;
        }

        struct sql_query *query = _query.front();
        _query.pop();
        unlock();

        const char *stmt = query->stmt;
        assert( "empty sql statement",stmt && query->size > 0 );

        if ( _sql.query ( stmt,query->size ) )
        {
            ERROR( "sql query error:%s",_sql.error() );
            ERROR( "sql will not exec:%s",stmt );
            
            invoke_cb( cb,query,NULL );
            continue;
        }
        
        /* 关服时不需要回调了 */
        if ( !cb )
        {
            invoke_cb( cb,query,NULL );
            continue;
        }
        
        struct sql_res *res = NULL;
        if ( _sql.result( &res ) )
        {
            ERROR( "sql result error[%s]:%s",stmt,_sql.error() );

            invoke_cb( cb,query,NULL );
            continue;
        }

        invoke_cb( cb,query,res );
    }
}

int32 lsql::stop()
{
    thread::stop();

    return 0;
}

int32 lsql::do_sql()
{
    if ( !active() )
    {
        return luaL_error( L,"sql thread not active" );
    }

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

    lock();
    if ( _query.empty() )  /* 不要使用_query.size() */
    {
        _notify = true;
    }
    _query.push( query );
    unlock();

    /* 子线程的socket是阻塞的。主线程检测到子线程正常处理sql则无需告知。防止
     * 子线程socket缓冲区满造成Resource temporarily unavailable
     */
    if ( _notify ) notify_child( MSG );

    return 0;
}

void lsql::notification( notify_t msg )
{
    switch ( msg )
    {
        case EXIT  : assert( "sql thread should not exit itself",false );abort();break;
        case ERROR :
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
        case MSG :
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

int32 lsql::next_result()
{
    lock();
    if ( _result.empty() )
    {
        unlock();
        lua_pushnil( L );
        return 1;
    }

    struct sql_result r = _result.front();
    _result.pop();
    unlock();

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
                    lua_pushint64( L,atoll(cols[col].value) );
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

bool lsql::cleanup()
{
    if ( _sql.ping() )
    {
        ERROR( "mysql ping fail at cleanup,data may lost:%s",_sql.error() );
        /* TODO write to file ? */
    }
    else
    {
        invoke_sql( false );
    }

    _sql.disconnect() ;
    mysql_thread_end();
    
    return true;
}

bool lsql::initlization()
{
    mysql_thread_init();
    if ( !_sql.connect() )
    {
        mysql_thread_end();
        notify_parent( ERROR );
        return false;
    }
    
    return true;
}

void lsql::invoke_cb( bool cb,struct sql_query *query,struct sql_res *res )
{
    if ( cb && query->callback )
    {
        sql_result result;
        result.id  = query->id;
        result.err = _sql.get_errno();
        result.res = res;

        lock();
        _result.push( result );
        unlock();

        notify_parent( MSG );
        
        delete query;
        return;
    }
    
    assert( "this sql should not have result",!res );
    delete query;
}
