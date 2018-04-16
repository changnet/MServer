#ifndef __LOG_H__
#define __LOG_H__

#include <map>
#include <queue>

#include "../global/global.h"

class log_one;

// 将日志内容分为小、中、大三种类型
typedef enum
{
    LOG_MIN = 0,
    LOG_MID = 1,
    LOG_MAX = 2
}log_size_t;

// 同一文件名日志类，避免同一文件多次打开、关闭
class log_file
{
public:
    log_file();
    ~log_file();

    void swap();
    bool valid();
    bool need_flush();
    int32 flush( const char *path );
    void push( const class log_one *one );
private:
    time_t _last_modify;  // 上一次修改的时间，如果太久没有写入日志，就需要清除

    // 一个主线程写入队列，一个日志线程写入文件队列，互不干扰
    std::queue<const class log_one *> *_queue;
    std::queue<const class log_one *> *_flush;
};

class log
{
public:
    static bool mkdir_p( const char *path );
public:
    void remove_empty();
    class log_one *allocate_one( size_t len );
    void deallocate_one( class log_one *one );

    class log_file *get_log_file( const char **path );
    int32 write( time_t tm,const char *path,const char *str,size_t len );
private:
    std::map< std::string,class log_file > _file_map;
    std::queue< class log_one *>[LOG_MAX+1] _mem_pool;
};

#endif /* __LOG_H__ */
