#ifndef __LOG_H__
#define __LOG_H__

#include <map>
#include <string>
#include <queue>

#include "../global/global.h"

class log;
class log_file
{
public:
    log_file();
    ~log_file();

    int32 flush();
    bool mark( int32 ts );
    bool valid( int32 ts );
    void push( time_t tm,const char *name,const char *str,size_t len = -1 );
private:
    int32 _ts;
    char _name[PATH_MAX];
    std::queue<const char *> *_queue;
    std::queue<const char *> *_flush;
};

class log
{
public:
    void remove_empty( int32 ts );
    class log_file *get_log_file( int32 ts );
    int32 write( time_t tm,const char *name,const char *str,size_t len = -1 );
private:
    std::map< std::string,class log_file > _map;
};

#endif /* __LOG_H__ */
