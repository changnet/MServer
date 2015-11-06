#include "sql.h"
#include <mysql/errmsg.h>

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

/* 线程主逻辑 */
void sql::routine()
{
    assert( "mysql connection not clean",NULL == conn );
    assert( "fd not valid",fd[0] > -1 && fd[1] > -1 );

    mysql_thread_init();
    conn = mysql_init( NULL );
    if ( !conn )
    {
        ERROR( "mysql init fail:%s\n",mysql_error(conn) );
        return;
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
         mysql_options( conn, MYSQL_SET_CHARSET_NAME , "utf8" ) ||
         mysql_options( conn, MYSQL_INIT_COMMAND,"SET autocommit=0" )
        )
    {
        ERROR( "mysql option fail:%s\n",mysql_error( conn ) );

        mysql_close( conn );
        mysql_thread_end();
        conn = NULL;

        return;
    }
    
    if ( NULL == mysql_real_connect( conn,ip,usr,pwd,db,port,NULL,0 ) )
    {
        ERROR( "mysql real connect fail:%s\n",mysql_error( conn ) );
        
        mysql_close( conn );
        mysql_thread_end();
        conn = NULL;

        return;
    }

    while ( _run )
    {
        if ( mysql_ping( conn ) )
        {
            ERROR( "mysql ping fail:%s\n",mysql_error( conn ) );
            continue;
        }
        
        uint64 event = 0;
        int32 sz = read( fd[1],&event,sizeof(uint64) ); /* 阻塞 */
        if 

        const char *statement = "SELECT * FROM society_join_log limit 1;";
        
        /* Client error message numbers are listed in the MySQL errmsg.h header
         * file. Server error message numbers are listed in mysqld_error.h
         */
        if ( mysql_real_query( conn,statement,strlen(statement) ) )
        {
            int32 err = mysql_errno( conn );
            if ( CR_SERVER_LOST == err || CR_SERVER_GONE_ERROR == err )
            {
                /* reconnect and try again */
                if ( mysql_ping( conn ) )
                {
                    ERROR( "mysql reconnect ping fail:%s\n",mysql_error( conn ) );
                    continue;
                }
                
                if ( mysql_real_query( conn,statement,strlen(statement) ) )
                {
                    ERROR( "mysql requery fail:%s\n",mysql_error( conn ) );
                    ERROR( "sql not exec:%s\n",statement );
                    continue;
                }
            }
            else
            {
                ERROR( "mysql query fail:%s\n",mysql_error( conn ) );
                ERROR( "sql not exec:%s\n",statement );
                continue;
            }
        }

        /* returns a null pointer if the statement did not return a result set
         * (for example, if it was an INSERT statement).An empty result set is
         * returned if there are no rows returned. (An empty result set differs
         * from a null pointer as a return value.)
         */
        MYSQL_RES *res = mysql_store_result( conn );
        if ( res )
        {
            MYSQL_ROW row;
            uint32 sz = mysql_num_fields(res);
            while ( (row = mysql_fetch_row(res)) )
            {
                char tmp[1024];
                for ( uint32 i = 0;i < sz;i ++ )
                {
                    snprintf( tmp,1024,"%s",row[i] );
                    PDEBUG( "result:%s\n",tmp );
                }
            }
            mysql_free_result( res );
        }
        else
        {
            ERROR( "mysql store result fail:%s\n",mysql_error( conn ) );
            ERROR( "sql not have result:%s\n",statement );
        }
    }
    
    mysql_close( conn );
    conn = NULL;

    mysql_thread_end();
}

// pthread_kill
