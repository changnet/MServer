/*
 * for epoll
 * 1.closing a file descriptor cause it to be removed from all
 *   poll sets automatically。（not dup fork,see man page)
 * 2.EPOLL_CTL_MOD操作可能会使得某些event被忽略(如当前有已有一个读事件，但忽然改为只监听
 *   写事件)，可能会导致这个事件丢失，尤其是使用ET模式时。
 * 3.采用pending、fdchanges、anfds队列，同时要保证在处理pending时(此时会触发很多事件)，不会
 *   更改pending、anfds队列，防止在循环过程中数组变化。但这时可能某个逻辑会把watcher的event
 *   更改，但pending这时仍触发，此时要检测事件是否为仍为当前所需。
 */


#ifndef __EV_H__
#define __EV_H__

#include "ev_def.h"
#include "ev_watcher.h"

class ev_loop
{
public:
    void run();
    void done();

private:
    volatile bool loop_done;
};

#endif /* __EV_H__ */
