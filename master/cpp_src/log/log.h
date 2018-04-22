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
    LOG_MAX = 2
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
    class log_one *allocate_one( size_t len );
    void deallocate_one( class log_one *one );

    int32 write_cache( time_t tm,const char *path,const char *str,size_t len );
private:
    log_one_list_t *_cache;   // 主线程写入缓存队列
    log_one_list_t *_flush;   // 日志线程写入文件队列
    log_one_list_t _mem_pool[LOG_MAX+1];  // 内存池，防止内存频繁分配
};

#endif /* __LOG_H__ */
