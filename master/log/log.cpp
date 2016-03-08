#include "log.h"

log_file::log_file()
{
    _name[0] = '\0';
}

log_file::~log_file()
{
}

void log_file::pop()
{
    assert( "pop empty log queue",!_queue.empty() );

    const char *str = _queue.front();
    assert( "empty log str pointer",str );
    
    _queue.pop();
    delete []str;
}

const char *log_file::front()
{
    if ( _queue.empty() ) return NULL;
    
    return _queue.front();
}

void log_file::push( const char *str,size_t len = -1 )
{
    assert( "push null log pointer",str );

    if ( len < 0 ) len = strlen( str );
    
    if ( len <= 0 )
    {
        ERROR( "empty log content\n" );
        return;
    }
    
    /* 自动换行 */
    static const char * const log_tail = "\n\0";
    static const size_t tail_len = strlen( log_tail );
    
    /* 截断太长的日志 */
    if ( LOG_MAX_LENGTH < len + tail_len ) len = LOG_MAX_LENGTH - tail_len;
    
    /* 浪费点内存，尝试避免内存碎片 */
    size_t mc = LOG_MIN_CHUNK;
    while ( mc < len ) mc *= 2;
    
    const char *_str = new [mc];
    memcpy( _str,str,len );
    memcpy( _str + len,log_tail,tail_len );
    
    _queue.push( _str );
}

/* ========================================================================== */

int32 log::do_write()
{
    /* 不能这样做，因为要加锁的 */
    std::map< std::string,class log_file >::iterator itr = _map.begin();
    while ( itr != _map.end() )
    {
        class log_file &lf = itr->second;
        lf.write_file();
    }
    
    _map.clear();
}

int32 log::write( const char *name,const char *str,size_t len )
{
    assert( "write log no file name",name );

    class log_file &lf = _map[name];
    lf.push( str,len );
    
    return 0;
}
