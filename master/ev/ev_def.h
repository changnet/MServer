#ifndef __EV_DEF_H__
#define __EV_DEF_H__

#include <stdexcept>
#include "../global/global.h"

#define MIN_TIMEJUMP  1. /* minimum timejump that gets detected (if monotonic clock available) */
#define MAX_BLOCKTIME 59.743 /* never wait longer than this time (to detect time jumps) */

/*
 * the heap functions want a real array index. array index 0 is guaranteed to not
 * be in-use at any time. the first heap entry is at array [HEAP0]. DHEAP gives
 * the branching factor of the d-tree.
 */

#define HEAP0 1
#define HPARENT(k) ((k) >> 1)
#define UPHEAP_DONE(p,k) (!(p))

typedef double ev_tstamp;

/* eventmask, revents, events... */
enum
{
    EV_UNDEF    = (int)0xFFFFFFFF, /* guaranteed to be invalid */
    EV_NONE     =            0x00, /* no events */
    EV_READ     =            0x01, /* ev_io detected read will not block */
    EV_WRITE    =            0x02, /* ev_io detected write will not block */
    EV_TIMER    =      0x00000100, /* timer timed out */
    EV_ERROR    = (int)0x80000000  /* sent when an error occurs */
};

#endif /* __EV_DEF_H__ */
