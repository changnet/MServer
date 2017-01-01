#ifndef __LMONGO_H__
#define __LMONGO_H__

#include <lua.hpp>
#include <queue>
#include <lbson.h>
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

    int32 find     ();
    int32 count    ();
    int32 insert   ();
    int32 update   ();
    int32 remove   ();
    int32 next_result();
    int32 find_and_modify();

    int32 self_callback ();
    int32 read_callback ();
    int32 error_callback();
private:
    bool cleanup();
    bool initlization();
    void routine( notify_t msg );
    void notification( notify_t msg );
    void invoke_command( bool cb = true );
private:
    lua_State *L ;
    class mongo _mongo;

    int32 ref_self;
    int32 ref_read;
    int32 ref_error;

    std::queue<mongons::query  *> _query ;
    std::queue<mongons::result *> _result;
};

#endif /* __LMONGO_H__ */
