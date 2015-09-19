#ifndef __BUFFER_H__
#define __BUFFER_H__

#include "../global/global.h"
#include "../pool/ordered_pool.h"

class buffer
{
public:
private:
    static class ordered_pool<BUFFER_CHUNK> allocator;
};

#endif /* __BUFFER_H__ */
