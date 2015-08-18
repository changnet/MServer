#include "config.h"              /* must be first include */
#include "global/global.h"

int main()
{
    atexit(onexit);

    uint64 u64 = 0;
    int64  i64 = 0;
    std::cout << "hello" << sizeof(u64) << sizeof(i64) << sizeof(int32) << std::endl;
    uint64 *x = new uint64[5];
    uint8 *y = new uint8;


    return 0;
}
