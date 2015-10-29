#ifndef __LEVENTLOOP_H__
#define __LEVENTLOOP_H__

/* 后台工作类
 * socket自杀
 * 假如客户端有数条消息到达，我们收到后在一个while循环中处理，然后在第一条消息中关闭socket
 * (比如数据验证不通过)并直接delete掉socket。可是，while循环还在进行，于是程序当掉。又或者
 * 我们发送完数据后需要关闭socket，但其实数据只是放到sending队列中而已。如果这时delete掉
 * socket对象，则无法完成数据发送。
 *
 * socket和timer需要一个队列来管理，并且这个队列可以根据fd或id快速存取。因为我们并不会把
 * socket或timer对象抛给lua管理。抛给lua管理会造成很多麻烦，比如无法防止socket自杀，无
 * 法设置sendings队列。我们只给lua一个id，backend类作为接口。lua只能告诉它想做什么，然
 * 后backend根据id安全地把工作完成。
 */

#include <lua.hpp>

#include "../global/global.h"
#include "../ev/ev.h"
#include "../ev/ev_watcher.h"
#include "../net/socket.h"

class leventloop : public ev_loop
{
public:
    static leventloop *instance();
    static void uninstance();
    
    explicit leventloop( lua_State *L );
    ~leventloop();
    
    int32 exit();
    int32 run ();
    int32 time();
    
    int32 signal();
    int32 set_signal_ref();
private:
    explicit leventloop( lua_State *L,bool singleton );
    static void sig_handler( int32 signum );
    void invoke_signal();
private:
    lua_State *L;
    int32 sig_ref;
    static uint32 sig_mask;
    static class leventloop *_loop;
};

#endif /* __LEVENTLOOP_H__ */
