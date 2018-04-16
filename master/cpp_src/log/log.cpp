#include <sys/stat.h>
#include <sys/types.h>

#include <string>

#include "log.h"

// 单次写入的日志内容
struct log_one
{
public:
    time_t _tm;
    size_t _len;

    virtual log_size_t get_type() = 0;
    virtual const char *get_ctx() = 0;
    virtual void set_ctx( const char *ctx,size_t len ) = 0;
};

template<log_size_t lst,size_t size>
class log_one_ctx : public log_one
{
public:
    log_size_t get_type() { return lst; }
    const char *get_ctx() { return _context; }
    void set_ctx( const char *ctx,size_t len )
    {
        size_t sz = len > size ? size : len;

        _len = sz;
        memcpy( _context,ctx,sz );
    }
private:
    char _context[size];
};

log_file::log_file()
{
    _last_modify = 0;
    _queue = new std::queue<const class log_one *>;
    _flush = new std::queue<const class log_one *>;
}

log_file::~log_file()
{
    assert( "log file not flush yet",_queue->empty() && _flush->empty() );
}

void log_file::swap( int32 ts )
{
    if ( !_flush.empty() )
    {
        ERROR( "log file swap not empty" );
        return false;
    }

    std::queue<const class log_one *> swap_tmp = _flush;

    _flush = _queue;
    _queue = swap_tmp;
}

void log_file::push( const class log_one *one )
{
    // 如果超过最大值，新来的就要丢日志了
    if ( _queue.size() >= LOG_MAX_COUNT )
    {
        ERROR( "log file over max,abort" );
        return;
    }

    _queue.push( one );
}


int32 log_file:flush_one( FILE *pf,const struct log_one *one )
{
    struct tm *ntm = localtime( one->_tm );
    int byte = fprintf( pf,
        "[%04d-%02d-%02d %02d:%02d:%02d]",(ntm->tm_year + 1900),
        (ntm->tm_mon + 1), ntm->tm_mday, ntm->tm_hour, ntm->tm_min,ntm->tm_sec
    );

    if ( byte <= 0 )
    {
        ERROR( "log file write time error" );
        return -1;
    }

    size_t wbyte = fwrite( one->_context,1,one->_len,pf );
    if ( wbyte <= 0 )
    {
        ERROR( "log file write context error" );
        return -1;
    }

    /* 自动换行 */
    static const char * tail = "\n";
    size_t tbyte = fwrite( tail,1,strlen( tail ),pf );
    if ( tbyte <= 0 )
    {
        ERROR( "log file write context error" );
        return -1;
    }

    return 0;
}

// 写入缓存的日志到文件
int32 log_file::flush( const char *path )
{
    FILE * pf = fopen( path, "ab+" );
    if ( !pf )  /* 无法打开文件，尝试写入stderr */
    {
        ERROR( "can't open log file(%s):%s\n", path,strerror(errno) );

        return -1;
    }

    while( !_flush->empty() )
    {
        const struct log_one *one = _flush->front();
        // TODO:出错了，也没有办法啊，暂时缓存一下。
        if ( flush_one() < 0 ) break;

        _flush->pop();
    }

    fclose( pf );
    return 0;
}

/* 如果太久该文件无日志写入，则不要再缓存了 */
bool log_file::valid( int32 ts )
{
    if ( !_flush->empty() || !_queue->empty() ) return false;
    if ( _last_modify + LOG_FILE_TIMEOUT > ::time() ) return false;

    return true;
}

/* 是否需要写入文件 */
bool log_file::need_flush()
{
    return !_flush.empty();
}

/* ========================================================================== */

/* 此函数上层会加锁，要求尽量快 */
class log_file *log::get_log_file( const char **path )
{
    std::map< std::string,class log_file >::iterator itr = _file_map.begin();
    while ( itr != _file_map.end() )
    {
        class log_file &lf = itr->second;
        if ( lf.need_flush() )
        {
            *path = itr->first.c_str();
            return &lf;
        }

        ++itr;
    }

    return NULL;
}

int32 log::write( time_t tm,const char *path,const char *str,size_t len )
{
    assert( "write log no file path",path );

    class log_one *one = allocate_one( len );
    if ( !one )
    {
        ERROR( "log write cant not allocate memory" );
        return -1;
    }

    one->_tm = tm;
    one->

    class log_file &lf = _file_map[path];
    lf.push( tm,path,str,len );

    return 0;
}

void log::remove_empty( int32 ts )
{
    bool rfl = true;
    while ( rfl )
    {
        rfl = false;
        std::map< std::string,class log_file >::iterator itr = _file_map.begin();
        while ( itr != _file_map.end() )
        {
            class log_file &lf = itr->second;

            /* 通过次数来控制同一个文件一轮只写入一次 */
            if ( !lf.valid( ts ) )
            {
                _file_map.erase( itr );
                rfl = true;
                break;
            }

            ++itr;
        }
    }
}

/* same as mkdir -p path */
bool log::mkdir_p( const char *path )
{
    assert( "mkdir_p:null path",path );

    char dir_path[PATH_MAX];
    int32 len = strlen( path );

    for ( int32 i = 0; i <= len && i < PATH_MAX; i++ )
    {
        dir_path[i] = *( path + i );
        if ( ('\0' == dir_path[i] || dir_path[i] == '/') && i > 0)
        {
            dir_path[i]='\0';
            if ( ::access(dir_path, F_OK) < 0 ) /* test if file already exist */
            {
                if ( ::mkdir( dir_path, S_IRWXU ) < 0 )
                {
                    ERROR( "mkdir -p %s fail:%s", dir_path, strerror(errno) );
                    return false;
                }
            }
            dir_path[i]='/';
        }
    }

    return true;
}
