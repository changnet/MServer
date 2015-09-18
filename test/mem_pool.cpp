#include <iostream>
#include <cstdio>
#include <ctime>
#include <cstdlib>

#include "ordered_pool.h"

void memory_fail()
{
    std::cerr << "no memory anymore !!!" << std::endl;
    exit( 1 );
}

int main()
{
    std::set_new_handler( memory_fail );

    ordered_pool<8192> pool;
    
    clock_t start = clock();
    
    char *(list[4][10000]) = {0};
    for ( int i = 0;i < 10000;i ++ )
    {
        list[0][i] = pool.ordered_malloc( 1 );
        list[1][i] = pool.ordered_malloc( 2 );
        list[3][i] = pool.ordered_malloc( 4 );
    }
    for ( int i = 0;i < 10000;i ++ )
    {
        pool.ordered_free( list[0][i],1 );
        pool.ordered_free( list[1][i],2 );
        pool.ordered_free( list[3][i],4 );
    }
    std::cout << float(clock() - start)/CLOCKS_PER_SEC << std::endl;

    return 0;
}
