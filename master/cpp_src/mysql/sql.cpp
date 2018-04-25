#include "sql.h"
#include <errmsg.h>

/* Call mysql_library_init() before any other MySQL functions. It is not
 * thread-safe, so call it before threads are created, or protect the call with
 * a mutex
 */
void sql::library_init()
{
    int32 ecode = mysql_library_init( 0,NULL,NULL );
    assert( "mysql library init fail", 0 == ecode );
}

/* 释放mysql库，仅在程序结束时调用。如果不介意内存，可以不调用.
 * mysql++就未调用
 */
void sql::library_end()
{
    mysql_library_end();
}

sql::sql()
{
    _is_cn = false;
    _conn =  NULL;

    _host[0]   = '\0';
    _usr[0]    = '\0';
    _pwd[0]    = '\0';
    _dbname[0] = '\0';

    _port = 0;
}

sql::~sql()
{
    assert( "sql not clean",NULL == _conn );
}

void sql::set( const char *host,const int32 port,
        const char *usr,const char *pwd,const char *dbname )
{
    /* 将数据复制一份，允许上层释放对应的内存 */
    _port = port;
    snprintf( _host  ,SQL_VAR_LEN,"%s",host   );
    snprintf( _usr   ,SQL_VAR_LEN,"%s",usr    );
    snprintf( _pwd   ,SQL_VAR_LEN,"%s",pwd    );
    snprintf( _dbname,SQL_VAR_LEN,"%s",dbname );
}

/* 连接数据库 */
int32 sql::connect()
{
    assert( "mysql connection not clean",NULL == _conn );

    _conn = mysql_init( NULL );
    if ( !_conn )
    {
        ERROR( "mysql init fail:%s\n",mysql_error(_conn) );
        return 1;
    }

    /* mysql_options的时间精度都为秒级
     */
    uint32 connect_timeout = 60;
    uint32 read_timeout    = 30;
    uint32 write_timeout   = 30;
    bool reconnect = true;
    if ( mysql_options( _conn, MYSQL_OPT_CONNECT_TIMEOUT, &connect_timeout ) ||
         mysql_options( _conn, MYSQL_OPT_READ_TIMEOUT, &read_timeout ) ||
         mysql_options( _conn, MYSQL_OPT_WRITE_TIMEOUT, &write_timeout ) ||
         mysql_options( _conn, MYSQL_OPT_RECONNECT, &reconnect ) ||
         mysql_options( _conn, MYSQL_SET_CHARSET_NAME , "utf8" )
        /*|| mysql_options( conn, MYSQL_INIT_COMMAND,"SET autocommit=0" ) */
        )
    {
        ERROR( "mysql option fail:%s\n",mysql_error( _conn ) );

        mysql_close( _conn );
        _conn =         NULL;

        return 1;
    }

    return raw_connect();
}

// 连接逻辑
int32 sql::raw_connect()
{
    /* CLIENT_REMEMBER_OPTIONS:Without this option, if mysql_real_connect()
     * fails, you must repeat the mysql_options() calls before trying to connect
     * again. With this option, the mysql_options() calls need not be repeated
     */
    if ( mysql_real_connect(
        _conn,_host,_usr,_pwd,_dbname,_port,NULL,CLIENT_REMEMBER_OPTIONS ) )
    {
        _is_cn = true;
        return 0;
    }

    /* 在实际应用中，允许mysql先不开启或者网络原因连接不上，不断重试
     */
    uint32 eno = mysql_errno( _conn );
    if ( CR_SERVER_LOST == eno || CR_CONN_HOST_ERROR == eno )
    {
        ERROR( "mysql will try again:%s\n",mysql_error( _conn ) );
        return -1;
    }

    ERROR( "mysql real connect fail:%s\n",mysql_error( _conn ) );

    mysql_close( _conn );
    _conn =         NULL;

    return 1;
}

void sql::disconnect()
{
    assert( "try to disconnect a invalid mysql connection",_conn );
    if ( !_conn ) return;

    mysql_close( _conn );
    _conn =         NULL;
}

int32 sql::ping()
{
    assert( "try to ping a invalid mysql connection",_conn );
    if ( !_conn )
    {
        ERROR( "mysql ping invalid connection" );
        return 1;
    }

    // 初始化时连接不成功，现在重新尝试
    if (  !_is_cn )
    {
        int32 ok = raw_connect();
        if ( -1 == ok )
        {
            return -1;// 需要继续尝试
        }
        else if ( ok > 0 )
        {
            return 1;// 错误
        }
        _is_cn = true;
    }

    int32 eno = mysql_ping( _conn );
    if ( eno )
    {
        ERROR( "mysql ping error:%s\n",mysql_error( _conn ) );
    }

    return eno;
}

const char *sql::error()
{
    assert( "try to get error from a invalid mysql connection",_conn );
    return mysql_error( _conn );
}

int32 sql::query( const char *stmt,size_t size )
{
    assert( "sql query,connection not valid",_conn );

    /* Client error message numbers are listed in the MySQL errmsg.h header
     * file. Server error message numbers are listed in mysqld_error.h
     */
    if ( mysql_real_query( _conn,stmt,size ) )
    {
        int32 ecode = mysql_errno( _conn );
        if ( CR_SERVER_LOST == ecode || CR_SERVER_GONE_ERROR == ecode )
        {
            /* reconnect and try again */
            if ( mysql_ping( _conn ) )
            {
                return mysql_errno( _conn );
            }

            if ( mysql_real_query( _conn,stmt,size ) )
            {
                return mysql_errno( _conn );
            }
        }
        else
        {
            return mysql_errno( _conn );
        }
    }

    return 0; /* same as mysql_real_query,return 0 if success */
}

int32 sql::result( struct sql_res **res )
{
    assert( "sql result,connection not valid",_conn );

    /* mysql_store_result() returns a null pointer if the statement did not
     * return a result set (for example, if it was an INSERT statement).
     * also returns a null pointer if reading of the result set
     * failed. You can check whether an error occurred by checking whether
     * mysql_error() returns a nonempty string, mysql_errno() returns nonzero
     * it does not do any harm or cause any notable performance degradation if
     * you call mysql_store_result() in all cases(INSERT,UPDATE ...)
     */
    MYSQL_RES *result = mysql_store_result( _conn );
    if ( result )
    {
        uint32 num_rows   = mysql_num_rows( result );
        if ( 0 >= num_rows ) /* we got empty set */
        {
            mysql_free_result( result );

            return 0; /* success */
        }

        uint32 num_fields = mysql_num_fields( result );
        assert( "mysql result field count zero",num_fields > 0 );

        /* 注意使用resize来避免内存重新分配以及push_back产生的拷贝消耗 */
        *res = new sql_res();
        (*res)->_num_rows = num_rows;
        (*res)->_num_cols = num_fields;
        (*res)->_fields.resize( num_fields );
        (*res)->_rows.resize  ( num_rows   );

        uint32 index = 0;
        MYSQL_FIELD *field;
        while( (field = mysql_fetch_field( result )) )
        {
            assert( "fetch field more than field count",index < num_fields );
            (*res)->_fields[index]._type = field->type;
            snprintf(
                (*res)->_fields[index]._name,SQL_FIELD_LEN,"%s",field->name );

            ++index;
        }

        MYSQL_ROW row;

        index = 0;
        while ( (row = mysql_fetch_row(result)) )
        {
            struct sql_row &res_row = (*res)->_rows[index];
            res_row._cols.resize( num_fields );

            /* mysql_fetch_lengths() is valid only for the current row of the
             * result set
             */
            size_t *lengths = mysql_fetch_lengths( result );
            for ( uint32 i = 0;i < num_fields;i ++ )
            {
                res_row._cols[i].set( row[i],lengths[i] );
            }

            ++index;
        }
        mysql_free_result( result );

        return 0; /* success */
    }

    return mysql_errno( _conn );
}

uint32 sql::get_errno()
{
    assert( "sql get_errno,connection not valid",_conn );
    return mysql_errno( _conn );
}
