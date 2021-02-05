#pragma once

#include <ctime>
#include <vector>

#include "../global/types.hpp"

class Pool;
class LogOne;

// 日志输出类型
enum LogType
{
    LT_NONE    = 0,
    LT_FILE    = 1, // 输出到指定文件
    LT_LPRINTF = 2, // 用于lua实现异步PRINTF宏定义
    LT_MONGODB = 3, // mongodb日志
    LT_CPRINTF = 4, // C异步PRINTF宏定义

    LO_MAX
};

class Log
{
public:
    // 将日志内容分为小、中、大三种类型。避免内存碎片化
    typedef enum
    {
        LS_S = 0, // small
        LS_M = 1, // middle
        LS_L = 2, // large

        LS_MAX
    } LogSize;

    // 单次写入的日志
    typedef std::vector<class LogOne *> LogOneList;

public:
    Log();
    ~Log();

    bool swap();
    void flush();
    void close_files();
    void collect_mem();
    size_t pending_size();
    bool inline empty() const { return _flush->empty(); }
    int32_t write_cache(time_t tm, const char *path, const char *ctx,
                        size_t len, LogType out);

private:
    class LogOne *allocate_one(size_t len);
    void deallocate_one(class LogOne *one);
    bool flush_one_file(struct tm &ntm, const LogOne *one, const char *path,
                        const char *prefix = "");
    int32_t flush_one_ctx(FILE *pf, const LogOne *one, struct tm &ntm,
                          const char *prefix);

private:
    LogOneList *_cache; // 主线程写入缓存队列
    LogOneList *_flush; // 日志线程写入文件队列

    std::unordered_map<std::string, FILE *> _files;
    class Pool *_ctx_pool[LS_MAX];
};
