#include "sql.h"

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
}

sql::~sql()
{
    assert( "sql thread not clean",NULL == conn );
}

/* 线程启动函数 */
bool sql::start( const char *_ip,const int32 _port,const char *_usr,
    const char *_pwd,const char *_db )
{
    /* 将数据复制一份，允许上层释放对应的内存 */
    port = _port;
    snprintf( ip ,SQL_VAR_LEN,"%s",_ip  );
    snprintf( usr,SQL_VAR_LEN,"%s",_usr );
    snprintf( pwd,SQL_VAR_LEN,"%s",_pwd );
    snprintf( db ,SQL_VAR_LEN,"%s",_db  );

    return thread::start();
}

/* 线程主逻辑 */
void sql::routine()
{
    mysql_thread_init();
    conn = mysql_init( NULL );
    if ( !conn )
    {
        ERROR( "mysql init fail:%s\n",mysql_error(conn) );
        return;
    }

    while ( _run )
    {
        PDEBUG( "i am alive %lu \n",pthread_self() );
        sleep( 1 );
    }
    
    mysql_close( conn );
    conn = NULL;

    mysql_thread_end();
}

// pthread_kill
