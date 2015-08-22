#ifndef __EV_DEF_H__
#define __EV_DEF_H__

#include <stdexcept>
#include "../global/global.h"

#ifdef __cplusplus
# if __cplusplus >= 201103L
#  define EV_THROW noexcept
# else
#  define EV_THROW EV_THROW
# endif
#else
# define EV_THROW
#endif

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

/* fd mask */
enum
{
    EV_FDUNDEF    = 0x00,
    EV_FDADD      = 0x01,
    EV_FDMODFY    = 0x02,
};

#endif /* __EV_DEF_H__ */
