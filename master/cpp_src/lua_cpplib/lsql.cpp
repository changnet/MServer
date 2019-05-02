#include "lsql.h"
#include "ltools.h"
#include "../system/static_global.h"

lsql::lsql( lua_State *L ) : thread("lsql")
{
    _valid = -1;
    _dbid = luaL_checkinteger( L,2 );
}

lsql::~lsql()
{
}

// 是否有效(只判断是否连接上，后续断线等不检测)
int32 lsql::valid ( lua_State *L )
{
    lua_pushinteger( L,_valid );

    return 1;
}

/* 连接mysql并启动线程 */
int32 lsql::start( lua_State *L )
{
    if ( active() )
    {
        return luaL_error( L,"sql thread already active" );
    }

    const char *host   = luaL_checkstring  ( L,1 );
    const int32 port   = luaL_checkinteger ( L,2 );
    const char *usr    = luaL_checkstring  ( L,3 );
    const char *pwd    = luaL_checkstring  ( L,4 );
    const char *dbname = luaL_checkstring  ( L,5 );

    _sql.set( host,port,usr,pwd,dbname );

    thread::start( 5 );  /* 10s ping一下mysql */
    return 0;
}

void lsql::routine( notify_t notify )
{
    /* 如果某段时间连不上，只能由下次超时后触发
     * 超时时间由thread::start参数设定
     */
    if ( 0 != _sql.ping() ) return;

    _valid = 1;
    invoke_sql();
}

const struct sql_query *lsql::pop_query()
{
    const struct sql_query *query = NULL;

    lock();
    if ( !_query.empty() )
    {
        query = _query.front();
        _query.pop();
    }
    unlock();

    return query;
}

void lsql::invoke_sql( bool is_return )
{
    const struct sql_query *query = NULL;

    while ( ( query = pop_query() ) )
    {
        const char *stmt = query->_stmt;
        assert( "empty sql statement",stmt && query->_size > 0 );

        struct sql_res *res = NULL;
        if ( expect_false( _sql.query( stmt,query->_size ) ) )
        {
            ERROR( "sql query error:%s",_sql.error() );
            ERROR( "sql will not exec:%s",stmt );
        }
        else
        {
            /* 对于select之类的查询，即使不需要回调，也要取出结果
             * 不然将会导致连接不同步
             */
            if ( _sql.result( &res ) )
            {
                ERROR( "sql result error[%s]:%s",stmt,_sql.error() );
            }
        }

        /* 关服的时候不需要回调到脚本
         * 在cleanup的时候可能脚本都被销毁了
         */
        if ( is_return && query->_id > 0 )
        {
            push_result( query->_id,res );
        }
        else
        {
            delete res;
        }

        delete query;
        query = NULL;
    }
}

int32 lsql::stop( lua_State *L )
{
    _valid = -1;
    thread::stop();

    return 0;
}

void lsql::push_query( const struct sql_query *query )
{
    lock();
    bool notify = _query.empty() ? true : false;
    _query.push( query );
    unlock();

    /* 子线程的socket是阻塞的。主线程检测到子线程正常处理sql则无需告知。防止
     * 子线程socket缓冲区满造成Resource temporarily unavailable
     */
    if ( notify ) notify_child( NTF_CUSTOM );
}

int32 lsql::do_sql( lua_State *L )
{
    if ( !active() )
    {
        return luaL_error( L,"sql thread not active" );
    }

    size_t size = 0;
    int32 id = luaL_checkinteger( L,1 );
    const char *stmt = luaL_checklstring( L,2,&size );
    if ( !stmt || size == 0 )
    {
        return luaL_error( L,"sql select,empty sql statement" );
    }


    struct sql_query *query = new sql_query( id,size,stmt );

    push_query( query );
    return 0;
}

void lsql::notification( notify_t notify )
{
    if ( NTF_CUSTOM == notify )
    {
        invoke_result();
    }
    else if ( NTF_ERROR == notify )
    {
        ERROR( "sql thread error" );
    }
    else
    {
        assert( "unknow sql event",false );
    }
}

int32 lsql::pop_result( struct sql_result &res )
{
    lock();
    if ( _result.empty() )
    {
        unlock();
        return 0;
    }

    res = _result.front();
    _result.pop();
    unlock();

    return 1;
}

void lsql::invoke_result()
{
    static lua_State *L = static_global::state();
    lua_pushcfunction( L,traceback );

    /* sql_result是一个比较小的结构体，因此不使用指针 */
    struct sql_result res;
    while ( pop_result( res ) )
    {
        lua_getglobal( L,"mysql_read_event" );
        lua_pushinteger( L,_dbid );
        lua_pushinteger( L,res._id );
        lua_pushinteger( L,res._ecode );

        int32 nargs = 3;
        int32 args  = mysql_to_lua( L,res._res );
        if ( args > 0 )         nargs += args;

        if ( LUA_OK != lua_pcall( L,nargs,0,1 ) )
        {
            ERROR( "sql call back error:%s",lua_tostring( L,-1 ) );
            lua_pop(L,1); /* remove error message */
        }

        delete res._res;
        res._res = NULL;
    }
    lua_pop(L,1); /* remove traceback */
}

int32 lsql::field_to_lua( lua_State *L,
    const struct sql_field &field,const struct sql_col &col )
{
    lua_pushstring( L,field._name );
    switch ( field._type )
    {
        case MYSQL_TYPE_TINY      :
        case MYSQL_TYPE_SHORT     :
        case MYSQL_TYPE_LONG      :
        case MYSQL_TYPE_TIMESTAMP :
        case MYSQL_TYPE_INT24     :
            lua_pushinteger( L,static_cast<LUA_INTEGER>(atoi(col._value)) );
            break;
        case MYSQL_TYPE_LONGLONG  :
            lua_pushint64( L,atoll(col._value) );
            break;
        case MYSQL_TYPE_FLOAT   :
        case MYSQL_TYPE_DOUBLE  :
        case MYSQL_TYPE_DECIMAL :
            lua_pushnumber( L,static_cast<LUA_NUMBER>(atof(col._value)) );
            break;
        case MYSQL_TYPE_VARCHAR     :
        case MYSQL_TYPE_TINY_BLOB   :
        case MYSQL_TYPE_MEDIUM_BLOB :
        case MYSQL_TYPE_LONG_BLOB   :
        case MYSQL_TYPE_BLOB        :
        case MYSQL_TYPE_VAR_STRING  :
        case MYSQL_TYPE_STRING      :
            lua_pushlstring( L,col._value,col._size );
            break;
        default :
            lua_pushnil( L );
            ERROR( "unknow mysql type:%d\n",field._type );
            break;
    }

    return 0;
}

/* 将mysql结果集转换为lua table */
int32 lsql::mysql_to_lua( lua_State *L,const struct sql_res *res )
{
    if ( !res ) return 0;

    assert( "sql result over boundary",
        res->_num_cols == res->_fields.size()
        &&res->_num_rows == res->_rows.size() );

    lua_createtable( L,res->_num_rows,0 ); /* 创建数组，元素个数为num_rows */

    const std::vector<sql_field> &fields = res->_fields;
    const std::vector<sql_row  > &rows   = res->_rows;
    for ( uint32 row = 0;row < res->_num_rows;row ++ )
    {
        lua_pushinteger( L,row + 1 ); /* lua table从1开始 */
        lua_createtable( L,0,res->_num_cols ); /* 创建hash表，元素个数为num_cols */

        const std::vector<sql_col> &cols = rows[row]._cols;
        for ( uint32 col = 0;col < res->_num_cols;col ++ )
        {
            assert( "sql result column "
                "over boundary",res->_num_cols == cols.size() );

            if ( !cols[col]._value ) continue;/* 值为NULL */

            field_to_lua( L,fields[col],cols[col] );
            lua_rawset( L, -3 );
        }

        lua_rawset( L, -3 );
    }

    return 1;
}

bool lsql::uninitialize()
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

bool lsql::initialize()
{
    mysql_thread_init();

    int32 ok = _sql.connect();
    if ( ok > 0 )
    {
        _valid = 0;
        mysql_thread_end();
        notify_parent( NTF_ERROR );
        return false;
    }
    else if ( -1 == ok )
    {
        // 初始化正常，但是需要稍后重试
        return true;
    }

    _valid = 1;
    return true;
}

void lsql::push_result( int32 id,struct sql_res *res )
{
    /* 需要回调的应该都有结果，没有的话可能是逻辑错误 */
    if ( !res )
    {
        ERROR( "sql query do not have result" );
    }

    struct sql_result result;

    result._id    = id;
    result._ecode = _sql.get_errno();
    result._res   = res;

    lock();
    bool notify = _result.empty() ? true : false;
    _result.push( result );
    unlock();

    if ( notify ) notify_parent( NTF_CUSTOM );
}
