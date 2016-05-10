#include "sql.h"
#include <errmsg.h>

/* Call mysql_library_init() before any other MySQL functions. It is not
 * thread-safe, so call it before threads are created, or protect the call with
 * a mutex
 */
void sql::library_init()
{
    assert( "mysql library init fail", 0 == mysql_library_init( 0,NULL,NULL ) );
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
    conn =  NULL;

    ip [0] = '\0';
    usr[0] = '\0';
    pwd[0] = '\0';
    db [0] = '\0';

    port = 0;
}

sql::~sql()
{
    assert( "sql not clean",NULL == conn );
}

void sql::set( const char *_ip,const int32 _port,const char *_usr,
    const char *_pwd,const char *_db )
{
    /* 将数据复制一份，允许上层释放对应的内存 */
    port = _port;
    snprintf( ip ,SQL_VAR_LEN,"%s",_ip  );
    snprintf( usr,SQL_VAR_LEN,"%s",_usr );
    snprintf( pwd,SQL_VAR_LEN,"%s",_pwd );
    snprintf( db ,SQL_VAR_LEN,"%s",_db  );
}

/* 连接数据库 */
bool sql::connect()
{
    assert( "mysql connection not clean",NULL == conn );

    conn = mysql_init( NULL );
    if ( !conn )
    {
        ERROR( "mysql init fail:%s\n",mysql_error(conn) );
        return false;
    }

    /* mysql_options的时间精度都为秒级
     */
    uint32 connect_timeout = 60;
    uint32 read_timeout    = 30;
    uint32 write_timeout   = 30;
    bool reconnect = true;
    if ( mysql_options( conn, MYSQL_OPT_CONNECT_TIMEOUT, &connect_timeout ) ||
         mysql_options( conn, MYSQL_OPT_READ_TIMEOUT, &read_timeout ) ||
         mysql_options( conn, MYSQL_OPT_WRITE_TIMEOUT, &write_timeout ) ||
         mysql_options( conn, MYSQL_OPT_RECONNECT, &reconnect ) ||
         mysql_options( conn, MYSQL_SET_CHARSET_NAME , "utf8" )
        /*|| mysql_options( conn, MYSQL_INIT_COMMAND,"SET autocommit=0" ) */
        )
    {
        ERROR( "mysql option fail:%s\n",mysql_error( conn ) );

        mysql_close( conn );
        conn = NULL;

        return false;
    }

    if ( NULL == mysql_real_connect( conn,ip,usr,pwd,db,port,NULL,0 ) )
    {
        ERROR( "mysql real connect fail:%s\n",mysql_error( conn ) );

        mysql_close( conn );
        conn = NULL;

        return false;
    }

    return true;
}

void sql::disconnect()
{
    assert( "try to disconnect a inactive mysql connection",conn );
    if ( !conn ) return;

    mysql_close( conn );
    conn = NULL;
}

int32 sql::ping()
{
    assert( "try to ping a inactive mysql connection",conn );

    return mysql_ping( conn );
}

const char *sql::error()
{
    assert( "try to get error from a inactive mysql connection",conn );
    return mysql_error( conn );
}

int32 sql::query( const char *stmt,size_t size )
{
    assert( "sql query,connection not valid",conn );

    /* Client error message numbers are listed in the MySQL errmsg.h header
     * file. Server error message numbers are listed in mysqld_error.h
     */
    if ( mysql_real_query( conn,stmt,size ) )
    {
        int32 err = mysql_errno( conn );
        if ( CR_SERVER_LOST == err || CR_SERVER_GONE_ERROR == err )
        {
            /* reconnect and try again */
            if ( mysql_ping( conn ) )
            {
                return mysql_errno( conn );
            }

            if ( mysql_real_query( conn,stmt,strlen(stmt) ) )
            {
                return mysql_errno( conn );
            }
        }
        else
        {
            return mysql_errno( conn );
        }
    }

    return 0; /* same as mysql_real_query,return 0 if success */
}

int32 sql::result( struct sql_res **_res )
{
    assert( "sql result,connection not valid",conn );

    /* mysql_store_result() returns a null pointer if the statement did not
     * return a result set (for example, if it was an INSERT statement).
     * also returns a null pointer if reading of the result set
     * failed. You can check whether an error occurred by checking whether
     * mysql_error() returns a nonempty string, mysql_errno() returns nonzero
     * it does not do any harm or cause any notable performance degradation if
     * you call mysql_store_result() in all cases(INSERT,UPDATE ...)
     */
    MYSQL_RES *res = mysql_store_result( conn );
    if ( res )
    {
        uint32 num_rows   = mysql_num_rows  ( res );
        if ( 0 >= num_rows ) /* we got empty set */
        {
            mysql_free_result( res );

            return 0; /* success */
        }

        uint32 num_fields = mysql_num_fields( res );
        assert( "mysql result field count zero",num_fields > 0 );

        /* 注意使用resize来避免内存重新分配以及push_back产生的拷贝消耗 */
        *_res = new sql_res();
        (*_res)->num_rows = num_rows;
        (*_res)->num_cols = num_fields;
        (*_res)->fields.resize( num_fields );
        (*_res)->rows.resize( num_rows );

        uint32 index = 0;
        MYSQL_FIELD *field;
        while( (field = mysql_fetch_field( res )) )
        {
            assert( "fetch field more than field count",index < num_fields );
            (*_res)->fields[index].type = field->type;
            snprintf( (*_res)->fields[index].name,SQL_FIELD_LEN,"%s",field->name );

            ++index;
        }

        MYSQL_ROW row;

        index = 0;
        while ( (row = mysql_fetch_row(res)) )
        {
            struct sql_row &_row = (*_res)->rows[index];
            _row.cols.resize( num_fields );

            /* mysql_fetch_lengths() is valid only for the current row of the
             * result set
             */
            size_t *lengths = mysql_fetch_lengths(res);
            for ( uint32 i = 0;i < num_fields;i ++ )
            {
                _row.cols[i].set( row[i],lengths[i] );
            }

            ++index;
        }
        mysql_free_result( res );

        return 0; /* success */
    }

    return mysql_errno( conn );
}

int32 sql::get_errno()
{
    assert( "sql get_errno,connection not valid",conn );
    return mysql_errno( conn );
}
