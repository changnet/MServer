#ifndef __IO_H__
#define __IO_H__

#include "../global/global.h"

/* socket input output control */
class io
{
public:
    virtual io();
    virtual ~io();

    virtual int32 recv();
    virtual int32 send();
};

#endif /* __IO_H__ */
