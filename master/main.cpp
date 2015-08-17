#include "config.h"              /* must be first include */
#include "global/global.h"

void test(...)
{
    printf( ... );
}

int main()
{
    uint64 u64 = 0;
    int64  i64 = 0;
    std::cout << "hello" << sizeof(u64) << sizeof(i64) << sizeof(int32) << std::endl;
    PDEBUG( "asfsabsdfe\n" );
    PDEBUG( "affefee %d\n",99875 );
    test( "abd" );
    
    return 0;
}
