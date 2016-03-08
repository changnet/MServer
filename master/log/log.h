#ifndef __LOG_H__
#define __LOG_H__

#include <map>
#include <queue>

#include "../global/global.h"

class log;
class log_file
{
public:
    log_file();
    ~log_file();

    void pop();
    const char *front();
    void push( const char *str,size_t len = -1 );
    
    inline bool empty() { return _queue.empty(); }
    
protected:
    void write_file();
private:
    char _name[PATH_MAX];
    std:queue<const char *> _queue;
};

class log
{
public:
    int32 do_write();
    int32 write( const char *name,const char *str,size_t len );
private:
    std:map< std::string,class log_file > _map;
};

#endif /* __LOG_H__ */
