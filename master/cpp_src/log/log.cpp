#include <sys/stat.h>
#include <sys/types.h>

#include <string>

#include "log.h"

// 单次写入的日志内容
class log_one
{
public:
    time_t _tm;
    size_t _len;
    char _path[PATH_MAX]; // TODO:这个路径是不是可以短一点，好占内存

    virtual log_size_t get_type() const = 0;
    virtual const char *get_ctx() const = 0;
    virtual void set_ctx( const char *ctx,size_t len ) = 0;
};

template<log_size_t lst,size_t size>
class log_one_ctx : public log_one
{
public:
    log_size_t get_type() const { return lst; }
    const char *get_ctx() const { return _context; }
    void set_ctx( const char *ctx,size_t len )
    {
        size_t sz = len > size ? size : len;

        _len = sz;
        memcpy( _context,ctx,sz );
    }
private:
    char _context[size];
};

log::log()
{
    _cache = new log_one_list_t();
    _flush = new log_one_list_t();
}

log::~log()
{
    assert( "log not flush",_cache->empty() && _flush->empty() );
}

// 交换缓存和待写入队列
bool log::swap()
{
    if (!_flush->empty() ) return false;

    log_one_list_t *swap_tmp = _flush;
    _flush = _cache;
    _cache = swap_tmp;

    return true;
}

// 主线程写入缓存，上层加锁
int32 log::write_cache( time_t tm,const char *path,const char *str,size_t len )
{
    assert( "write log no file path",path );

    class log_one *one = allocate_one( len );
    if ( !one )
    {
        ERROR( "log write cant not allocate memory" );
        return -1;
    }

    one->_tm = tm;
    one->set_ctx( str,len );
    snprintf( one->_path,PATH_MAX,"%s",path );

    _cache->push_back( one );
    return 0;
}

// 写入一项日志内容
int32 log::flush_one_ctx( FILE *pf,const struct log_one *one )
{
    struct tm ntm;
    localtime_r( &(one->_tm),&ntm );

    int byte = fprintf( pf,
        "[%04d-%02d-%02d %02d:%02d:%02d]",(ntm.tm_year + 1900),
        (ntm.tm_mon + 1), ntm.tm_mday, ntm.tm_hour, ntm.tm_min,ntm.tm_sec
    );

    if ( byte <= 0 )
    {
        ERROR( "log file write time error" );
        return -1;
    }

    size_t wbyte = fwrite( 
        (const void*)one->get_ctx(),sizeof(char),one->_len,pf );
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

// 写入一个日志文件
bool log::flush_one_file()
{
    FILE *pf = NULL;
    const char *path = NULL;
    log_one_list_t::iterator itr = _flush->begin();
    for ( ;itr != _flush->end(); ++itr )
    {
        log_one *one = *itr;
        if ( 0 == one->_len ) continue;

        // 第一次查找到可写入的日志时打开文件
        if ( NULL == path )
        {
            path = one->_path;
            pf = fopen( path, "ab+" );
            if ( !pf )  /* 无法打开文件*/
            {
                ERROR( "can't open log file(%s):%s\n", path,strerror(errno) );

                // TODO:这个异常处理有问题
                // 打开不了文件，可能是权限、路径、磁盘满，这里先全部标识为已写入，丢日志
                one->_len = 0;
                return true; 
            }
        }
        else
        {
            // 为了提高效率，一次只写入一个文件
            if ( 0 != strcmp( path,one->_path ) ) continue;
        }

        flush_one_ctx( pf,one );
        one->_len = 0; // 标记为已经写入
    }

    if ( pf ) fclose( pf );

    return NULL != pf;
}

// 日志线程写入文件
void log::flush()
{
    bool ok = true;
    do
    {
        ok = flush_one_file();
    }while( ok );
}

// 回收内存到内存池，上层加锁
void log::collect_mem()
{
    log_one_list_t::iterator itr = _flush->begin();
    for ( ;itr != _flush->end(); ++itr )
    {
        deallocate_one( *itr );
    }

    _flush->clear();
}

// 从内存池中分配一个日志缓存对象
class log_one *log::allocate_one( size_t len )
{
    return NULL;
}

//回收一个日志缓存对象到内存池
void log::deallocate_one( class log_one *one )
{
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
