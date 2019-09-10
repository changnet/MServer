#pragma once

#include <ctime>
#include <vector>

#include "../global/global.h"

class pool;
class log_one;

// 日志输出类型
typedef enum
{
    LO_FILE    = 1, // 输出到指定文件
    LO_LPRINTF = 2, // 用于lua实现异步PRINTF宏定义
    LO_MONGODB = 3, // mongodb日志
    LO_CPRINTF = 4, // C异步PRINTF宏定义

    LO_MAX
}log_out_t;

class log
{
public:
    // 将日志内容分为小、中、大三种类型。避免内存碎片化
    typedef enum
    {
        LOG_SIZE_S = 0, //small
        LOG_SIZE_M = 1, // middle
        LOG_SIZE_L = 2, // large

        LOG_SIZE_MAX
    }log_size_t;

    // 单次写入的日志
    typedef std::vector< class log_one *> log_one_list_t;
public:
    log();
    ~log();

    bool swap();
    void flush();
    void close_files();
    void collect_mem();
    size_t pending_size();
    bool inline empty() const { return _flush->empty(); }
    int32 write_cache( time_t tm,
        const char *path,const char *ctx,size_t len,log_out_t out );
private:
    class log_one *allocate_one( size_t len );
    void deallocate_one( class log_one *one );
    bool flush_one_file(struct tm &ntm,
        const log_one *one,const char *path,const char *prefix = "" );
    int32 flush_one_ctx(
        FILE *pf,const struct log_one *one,struct tm &ntm,const char *prefix );
private:
    log_one_list_t *_cache;   // 主线程写入缓存队列
    log_one_list_t *_flush;   // 日志线程写入文件队列

    map_t<std::string,FILE *> _files;
    class pool* _ctx_pool[LOG_SIZE_MAX];
};
