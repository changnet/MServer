#ifndef __LMONGO_H__
#define __LMONGO_H__

#include <queue>

#include "../thread/thread.h"
#include "../mongo/mongo.h"

struct lua_State;
class lmongo : public thread
{
public:
    explicit lmongo( lua_State *L );
    ~lmongo();

    int32 start();
    int32 stop ();
    int32 valid();

    int32 find     ();
    int32 count    ();
    int32 insert   ();
    int32 update   ();
    int32 remove   ();
    int32 find_and_modify();
private:
    /* for thread */
    bool cleanup();
    bool initlization();
    void routine( notify_t msg );
    void notification( notify_t msg );

    void invoke_result();
    void invoke_command( bool is_return = true );

    void push_query( const struct mongo_query *query );
    void push_result( const struct mongo_result *result );

    const struct mongo_result *pop_result();
    const struct mongo_query  *pop_query ();
private:
    lua_State *L ;
    class mongo _mongo;

    int32 _valid;
    int32 _dbid;

    std::queue<const struct mongo_query  *> _query ;
    std::queue<const struct mongo_result *> _result;
};

#endif /* __LMONGO_H__ */
