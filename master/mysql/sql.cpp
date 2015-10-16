#include "sql.h"

/* Call mysql_library_init() before any other MySQL functions. It is not
 * thread-safe, so call it before threads are created, or protect the call with
 * a mutex
 */
int32 sql::lib_init = mysql_library_init( 0,NULL,NULL );

/* 释放mysql库，仅在程序结束时调用。如果不介意内存，可以不调用.
 * mysql++就未调用
 */
void sql::purge()
{
    mysql_library_end();
}

sql::sql()
{
    assert( "mysql library init fail", 0 == lib_init );
    
    conn =  NULL;
    _run = false;
    thread_t = 0;
}

sql::~sql()
{
    assert( "sql thread not clean",NULL == conn );
}

/* 终止线程 */
void sql::stop()
{
    /* 由于_run在线程中只读不写，不考虑锁 */
    _run = false;
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
    PDEBUG( "create=====================\n");
    /* 创建线程 */
    if ( pthread_create( &thread_t,NULL,sql::start_routine,(void *)this ) )
    {
        ERROR( "sql thread create fail:%s\n",strerror(errno) );
        return false;
    }
    PDEBUG( "done=====================\n");
    return true;
}

/* 线程入口函数 */
void *sql::start_routine( void *arg )
{
    class sql *sql = static_cast<class sql *>( arg );
    assert( "sql start routine got NULL argument",sql );
    
    signal_block();  /* 子线程不处理外部信号 */
    sql->routine();
    
    return NULL;
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
    PDEBUG( "routine start -------------------\n");
    _run = true;
    while ( _run )
    {
        for ( int32 i = 0;i < 100;i ++ )
        {
            PDEBUG( "i am alive\n" );
            sleep( 1 );
        }
    }
    
    mysql_close( conn );
    conn = NULL;

    mysql_thread_end();
}

// pthread_kill
