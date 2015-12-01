#ifndef __LMONGO_H__
#define __LMONGO_H__

#include <lua.hpp>
#include <queue>
#include "../thread/thread.h"
#include "../ev/ev_watcher.h"
#include "../mongo/mongo.h"

class lmongo
{
public:
    explicit lmongo( lua_State *L );
    lmongo();
    
    int32 start();
    int32 stop ();
    int32 join ();

    int32 do_sql    ();
    int32 get_result();
    
    int32 self_callback ();
    int32 read_callback ();
    int32 error_callback();
    
    /*
    http://api.mongodb.org/c/current/mongoc_collection_t.html
    find_and_modify
    count
    select
    select_one
    insert
    update
    delete
    delete_all
    select_raw
    ensureindex
    */
private:
    
};

#endif /* __LMONGO_H__ */
