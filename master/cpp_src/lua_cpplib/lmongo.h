#pragma once

#include <queue>

#include "../thread/thread.h"
#include "../mongo/mongo.h"

// 由于指针可能是NULL，故用-1来表示。但是这并不百分百安全。不过在这里，顶多只是内存泄漏 sbrk有类似的用法
// The UNIX sbrk() function relies on this working,
// in that it returns -1 as a pointer value to indicate a particular situation
#define END_BSON (bson_t *) - 1

struct lua_State;
class LMongo : public Thread
{
public:
    ~LMongo();
    explicit LMongo( lua_State *L );

    int32_t start( lua_State *L );
    int32_t stop ( lua_State *L );
    int32_t valid( lua_State *L );

    int32_t find     ( lua_State *L );
    int32_t count    ( lua_State *L );
    int32_t insert   ( lua_State *L );
    int32_t update   ( lua_State *L );
    int32_t remove   ( lua_State *L );
    int32_t find_and_modify( lua_State *L );

    size_t busy_job( size_t *finished = NULL,size_t *unfinished = NULL );
private:
    /* for thread */
    bool uninitialize();
    bool initialize();
    void routine( NotifyType notify );
    void notification( NotifyType notify );

    void invoke_result();
    void invoke_command();

    void push_query( const struct MongoQuery *query );
    void push_result( const struct MongoResult *result );
    bson_t *string_or_table_to_bson(
        lua_State *L,int index,int opt = -1,bson_t *bs = END_BSON,... );

    const struct MongoResult *pop_result();
    const struct MongoQuery  *pop_query ();
private:
    class Mongo _mongo;

    int32_t _valid;
    int32_t _dbid;

    std::queue<const struct MongoQuery  *> _query ;
    std::queue<const struct MongoResult *> _result;
};
