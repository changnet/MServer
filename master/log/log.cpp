#include "log.h"

log_file::log_file()
{
    _ts = 0;
    _queue   = NULL;
    _flush   = NULL;
    _name[0] = '\0';
}

log_file::~log_file()
{
    assert( "log file not flush yet",!_queue && !_flush );
}

bool log_file::mark( int32 ts )
{
    if ( !_queue || _ts >= ts ) return false;

    _ts = ts;
    _flush = _queue;
    _queue = NULL;

    return true;
}

void log_file::push( time_t tm,const char *name,const char *str,size_t len )
{
    assert( "push null log pointer",str );
    
    if ( '\0' == _name[0] )
    {
        snprintf( _name,PATH_MAX,"%s",name );
    }

    if ( len < 0 ) len = strlen( str );
    
    if ( len <= 0 )
    {
        ERROR( "empty log content\n" );
        return;
    }
    
    /* 自动加上时间 */
    static char tm_str[256];
    struct tm *ntm = localtime( &tm );
    snprintf(tm_str,256,"[%04d-%02d-%02d %02d:%02d:%02d]",(ntm->tm_year + 1900),
        (ntm->tm_mon + 1), ntm->tm_mday, ntm->tm_hour, ntm->tm_min,
        ntm->tm_sec);
    size_t tm_len = strlen( tm_str );

    /* 自动换行 */
    static const char * const log_tail = "\n\0";
    static const size_t tail_len = strlen( log_tail ) + 1;
    
    /* 截断太长的日志 */
    if ( LOG_MAX_LENGTH < len + tail_len + tm_len )
        len = LOG_MAX_LENGTH - tail_len - tm_len;
    
    /* 浪费点内存，尝试避免内存碎片 */
    size_t mc = LOG_MIN_CHUNK;
    while ( mc < len ) mc *= 2;
    
    char *_str = new char[mc];
    memcpy( _str,tm_str,tm_len );
    memcpy( _str + tm_len,str,len );
    memcpy( _str + tm_len + len,log_tail,tail_len );
    
    if ( !_queue ) _queue = new std::queue<const char *>();
    
    _queue->push( _str );
}

int32 log_file::flush()
{
    assert( "log_file nothing to flush",_flush );

    FILE * pf = fopen( _name, "ab+" );
    if ( !pf )  /* 无法打开文件，尝试写入stderr */
    {
        fprintf( stderr, "can't open log file(%s):%s\n", _name,strerror(errno) );
        while( !_flush->empty() )
        {
            const char *str = _flush->front();
            _flush->pop();

            fprintf( stderr,"%s",str );
            delete []str;
        }
        
        delete _flush;
        _flush = NULL;

        return -1;
    }

    while( !_flush->empty() )
    {
        const char *str = _flush->front();
        _flush->pop();
        
        fwrite( str,1,strlen(str),pf );
        delete []str;
    }

    fclose( pf );
    delete _flush;
    _flush = NULL;
    
    return 0;
}

/* 如果太久该文件无日志写入，则无效 */
bool log_file::valid( int32 ts )
{
    if ( !_queue && ts > _ts + 10 ) return false;
    
    return true;
}

/* ========================================================================== */

/* 此函数上层会加锁，要求尽量快 */
class log_file *log::get_log_file( int32 ts )
{
    std::map< std::string,class log_file >::iterator itr = _map.begin();
    while ( itr != _map.end() )
    {
        class log_file &lf = itr->second;

        /* 通过次数来控制同一个文件一轮只写入一次 */
        if ( lf.mark( ts ) )
        {
            return &lf;
        }
        
        ++itr;
    }
    
    return NULL;
}

int32 log::write( time_t tm,const char *name,const char *str,size_t len )
{
    assert( "write log no file name",name );

    class log_file &lf = _map[name];
    lf.push( tm,name,str,len );
    
    return 0;
}

void log::remove_empty( int32 ts )
{
    bool rfl = true;
    while ( rfl )
    {
        rfl = false;
        std::map< std::string,class log_file >::iterator itr = _map.begin();
        while ( itr != _map.end() )
        {
            class log_file &lf = itr->second;

            /* 通过次数来控制同一个文件一轮只写入一次 */
            if ( !lf.valid( ts ) )
            {
                _map.erase( itr );
                rfl = true;
                break;
            }
            
            ++itr;
        }
    }
}
