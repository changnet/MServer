#ifndef __LOG_H__
#define __LOG_H__

#include <map>
#include <vector>

#include "../global/global.h"

class log_one;

// 将日志内容分为小、中、大三种类型
typedef enum
{
    LOG_MIN = 0,
    LOG_MID = 1,
    LOG_MAX = 2,

    LOG_SIZE_MAX
}log_size_t;

// 单次写入的日志
typedef std::vector< class log_one *> log_one_list_t;

class log
{
public:
    static bool mkdir_p( const char *path );
public:
    log();
    ~log();

    bool swap();
    void flush();
    void collect_mem();
    int32 write_cache( time_t tm,const char *path,const char *str,size_t len );
private:
    bool flush_one_file();
    void allocate_pool( log_size_t lt );
    class log_one *allocate_one( size_t len );
    void deallocate_one( class log_one *one );
    int32 flush_one_ctx( FILE *pf,const struct log_one *one );
private:
    log_one_list_t *_cache;   // 主线程写入缓存队列
    log_one_list_t *_flush;   // 日志线程写入文件队列
    log_one_list_t _mem_pool[LOG_SIZE_MAX];  // 内存池，防止内存频繁分配

    /* 日志分配内存大小*/
    static const size_t LOG_SIZE[LOG_SIZE_MAX];
    /* 日志内存池大小*/
    static const size_t LOG_POOL[LOG_SIZE_MAX];
    /* 内存池每次分配的大小 */
    static const size_t LOG_NEW[LOG_SIZE_MAX];
};

#endif /* __LOG_H__ */
