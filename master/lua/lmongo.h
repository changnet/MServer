#ifndef __LMONGO_H__
#define __LMONGO_H__

#include <lua.hpp>
#include <queue>
#include "../thread/thread.h"
#include "../ev/ev_watcher.h"
#include "../mongo/mongo.h"
#include "../mongo/mongo_def.h"

class lmongo : public thread
{
public:
    explicit lmongo( lua_State *L );
    ~lmongo();

    int32 start();
    int32 stop ();
    int32 join ();

    int32 find     ();
    int32 count    ();
    int32 next_result();
    int32 find_and_modify();

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
    ensureindex --> create_index
    */
private:
    enum
    {
        ERR  = -1,
        EXIT = 0 ,
        READ = 1
    };
private:
    void routine();
    void invoke_command();
    void mongo_cb( ev_io &w,int32 revents );
    void result_encode( bson_t *doc );
    void bson_encode( bson_iter_t &iter,bool is_array = false );
private:
    int32 fd[2]  ;
    lua_State *L ;
    ev_io watcher;
    class mongo _mongo;

    int32 ref_self;
    int32 ref_read;
    int32 ref_error;

    std::queue<mongons::query  *> _query ;
    std::queue<mongons::result *> _result;
};

#endif /* __LMONGO_H__ */
