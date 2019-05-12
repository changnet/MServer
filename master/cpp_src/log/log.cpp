#include <string>

#include "log.h"

/* 日志分配内存大小*/
const size_t log::LOG_SIZE[LOG_SIZE_MAX] = { 64,1024,LOG_MAX_LENGTH };
/* 日志内存池大小*/
const size_t log::LOG_POOL[LOG_SIZE_MAX] = { 512,512,1 };
/* 内存池每次分配的大小 */
const size_t log::LOG_NEW[LOG_SIZE_MAX] = { 64,64,1 };

// 单次写入的日志内容
class log_one
{
public:
    log_one(){};
    virtual ~log_one(){};

    time_t _tm;
    size_t _len;
    log_out_t _out;
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
        _context[len] = 0; // 有可能是输出到stdout的，必须0结尾
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

    delete _cache;
    delete _flush;

    _cache = NULL;
    _flush = NULL;

    for ( int idx = 0;idx < LOG_SIZE_MAX;++idx )
    {
        log_one_list_t *pool = &(_mem_pool[idx]);
        log_one_list_t::iterator itr = pool->begin();
        for ( ;itr != pool->end();++itr ) delete *itr;

        pool->clear();
    }
}

// 等待处理的日志数量
size_t log::pending_size()
{
    return _cache->size() + _flush->size();
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
int32 log::write_cache( time_t tm,
    const char *path,const char *ctx,size_t len,log_out_t out )
{
    assert( "write log no file path",path );

    class log_one *one = allocate_one( len + 1 );
    if ( !one )
    {
        ERROR( "log write cant not allocate memory" );
        return -1;
    }

    one->_tm = tm;
    one->_out = out;
    one->set_ctx( ctx,len );
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
bool log::flush_one_file( log_one_list_t::iterator pos )
{
    log_one *one = *pos;
    const char *path = one->_path;

    FILE *pf = fopen( path, "ab+" );
    if ( !pf )  /* 无法打开文件*/
    {
        ERROR( "can't open log file(%s):%s\n", path,strerror(errno) );

        // TODO:这个异常处理有问题
        // 打开不了文件，可能是权限、路径、磁盘满，这里先全部标识为已写入，丢日志
        return false;
    }

    do{
        one = *pos;
        if ( 0 == strcmp( path,one->_path ) )
        {
            flush_one_ctx( pf,one );
            one->_len = 0; // 标记为已经写入
        }

        ++pos;
    }while ( pos != _flush->end() );

    fclose( pf );
    return true;
}

// 日志线程写入文件
void log::flush()
{
    log_one_list_t::iterator itr = _flush->begin();
    for ( ;itr != _flush->end(); ++itr )
    {
        log_one *one = *itr;
        if ( 0 == one->_len ) continue;

        switch( one->_out )
        {
            case LO_FILE :
                flush_one_file( itr );
                break;
            case LO_LPRINTF :
                raw_cprintf_log( one->_tm,"LP","%s",one->get_ctx() );
                break;
            case LO_MONGODB :
                raw_mongodb_log( one->_tm,"CP","%s",one->get_ctx() );
                break;
            case LO_CPRINTF :
                raw_cprintf_log( one->_tm,"LP","%s",one->get_ctx() );
                break;
            default:
                ERROR("unknow log output type:%d",one->_out);
                break;

        }

        one->_len = 0;
    }
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

// 内存池分配逻辑
void log::allocate_pool( log_size_t lt )
{
#define ALLOCATE_MANY( LT,SZ )                                     \
    do{                                                            \
        for ( size_t idx = 0;idx < ctx_sz;++idx ){                 \
            pool->push_back( new log_one_ctx< LT,SZ >() );         \
        }                                                          \
    }while( 0 )

    assert(
        "log allocate pool illegal log size type",
        (lt >= LOG_MIN) && (lt < LOG_SIZE_MAX) );

    size_t ctx_sz = LOG_NEW[lt];
    log_one_list_t *pool = &(_mem_pool[lt]);

    // 由于这里没有使用oerder_pool，无法一次分配一个数组或者内存块，只能通过for来分配
    switch( lt )
    {
        case LOG_MIN : ALLOCATE_MANY( LOG_MIN,64 );break;
        case LOG_MID : ALLOCATE_MANY( LOG_MID,512 );break;
        case LOG_MAX : ALLOCATE_MANY( LOG_MAX,LOG_MAX_LENGTH );break;
        default: return;
    }
#undef ALLOCATE_MANY
}

// 从内存池中分配一个日志缓存对象
class log_one *log::allocate_one( size_t len )
{
    if ( len > LOG_MAX_LENGTH ) len = LOG_MAX_LENGTH;

    uint32 lt;
    for ( lt = LOG_MIN;lt < LOG_SIZE_MAX;++lt )
    {
        if ( len <= LOG_SIZE[lt] )
        {
            log_one_list_t *pool = &(_mem_pool[lt]);
            if ( expect_false(pool->empty()) )
            {
                allocate_pool( static_cast<log_size_t>(lt) );
            }

            // 需要重新检测vector是否为空
            if ( expect_true(!pool->empty()) )
            {
                class log_one *one = pool->back();

                pool->pop_back();
                return one;
            }

            return NULL;
        }
    }
    return NULL;
}

//回收一个日志缓存对象到内存池
void log::deallocate_one( class log_one *one )
{
    assert( "deallocate one NULL log ctx",one );

    log_size_t lt = one->get_type();
    if ( _mem_pool[lt].size() >= LOG_POOL[lt] )
    {
        delete one;
        return;
    }

    _mem_pool[lt].push_back( one );
}
