/*
 * loop member variable declarations
 */

#define VARx(type,name) VAR(name, type name)

VARx(ev_tstamp, now_floor) /* last time we refreshed rt_time */
VARx(ev_tstamp, mn_now)    /* monotonic clock "now" */
VARx(ev_tstamp, rtmn_diff) /* difference realtime - monotonic time */

/* for reverse feeding of events */
VARx(W *, rfeeds)
VARx(int, rfeedmax)
VARx(int, rfeedcnt)

VAR (pendings, ANPENDING *pendings)
VAR (pendingmax, int pendingmax)
VAR (pendingcnt, int pendingcnt)
VARx(ev_prepare, pending_w) /* dummy pending watcher */

VARx(ev_tstamp, io_blocktime)
VARx(ev_tstamp, timeout_blocktime)

#include <stdbool.h>
VARx(volatile bool, loop_done)  /* loop break flag */

VARx(int, backend_fd)
VARx(ev_tstamp, backend_mintime) /* assumed typical timer resolution */
VAR (backend_modify, void (*backend_modify)(EV_P, int fd, int oev, int nev))
VAR (backend_poll  , void (*backend_poll)(EV_P, ev_tstamp timeout))

VARx(ANFD *, anfds)
VARx(int, anfdmax)

/* for epoll */
VARx(struct epoll_event *, epoll_events)
VARx(int, epoll_eventmax)
VARx(int *, epoll_eperms)
VARx(int, epoll_epermcnt)
VARx(int, epoll_epermmax)


VARx(int *, fdchanges)
VARx(int, fdchangemax)
VARx(int, fdchangecnt)

VARx(ANHE *, timers)
VARx(int, timermax)
VARx(int, timercnt)

#undef VARx
