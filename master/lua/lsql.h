#ifndef __LSQL_H__
#define __LSQL_H__

#include <lua.hpp>
#include "../global/global.h"
#include "../thread/thread.h"
#include "../ev/ev_watcher.h"
#include "../mysql/sql.h"
#include "../pool/ordered_pool.h"

class lsql : public thread
{
public:
    explicit lsql( lua_State *L );
    ~lsql();

    int32 start();
    int32 stop ();
    int32 join ();

    int32 call  ();
    int32 del   ();
    int32 update();
    int32 select();
    int32 insert();
    
    /*
    START TRANSACTION [WITH CONSISTENT SNAPSHOT]
    BEGIN [WORK]
    COMMIT [WORK] [AND [NO] CHAIN] [[NO] RELEASE]
    ROLLBACK [WORK] [AND [NO] CHAIN] [[NO] RELEASE]
    */
    int32 begin();
    int32 commit();
    int32 rollback();
private:
    enum
    {
        ERR  = -1,
        EXIT = 0 ,
        READ = 1
    };
private:
    void routine();
    void do_sql ();
    void sql_cb( ev_io &w,int32 revents );
    
    /* 获取一个合适的buff
     * sql字段通过小于64(包括uuid、md5)
     */
    inline size_t make_size( size_t size )
    {
        size_t _size = SQL_CHUNK;
        while ( _size < size ) _size <<= 1;
        
        return _size;
    }
private:
    int32 fd[2]   ;
    lua_State *L  ;
    class sql _sql;
    ev_io watcher ;

    std::vector<sql_query> _query ;
    std::vector<sql_res *> _result;
    
    class ordered_pool<SQL_CHUNK> allocator;
};

#endif /* __LSQL_H__ */
