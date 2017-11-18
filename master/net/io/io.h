#ifndef __IO_H__
#define __IO_H__

#include "../buffer.h"

/* socket input output control */
class io
{
public:
    io();
    virtual ~io();

    virtual int32 recv( int32 fd,class buffer &buff ) const;
    virtual int32 send( int32 fd,class buffer &buff ) const;
};

#endif /* __IO_H__ */
