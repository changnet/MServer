#include <iostream>
#include <cstdio>
#include <ctime>
#include <cstdlib>

#include "ordered_pool.h"
//#include <boost/pool/singleton_pool.hpp>

#define CHUNK_SIZE    8

/*
2G ubuntu vm 10000times

xzc@xzc-VirtualBox:~/code/pool$ ./main
my pool run:0.035874
xzc@xzc-VirtualBox:~/code/pool$ ./main
my pool run:0.035968
xzc@xzc-VirtualBox:~/code/pool$ ./main
my pool run:0.027455
xzc@xzc-VirtualBox:~/code/pool$ ./main
my pool run:0.03688

boost run:8.42273
xzc@xzc-VirtualBox:~/code/pool$ ./main
boost run:8.50574
xzc@xzc-VirtualBox:~/code/pool$ ./main
boost run:8.48862

std run:0.004238
xzc@xzc-VirtualBox:~/code/pool$ ./main
std run:0.003537
xzc@xzc-VirtualBox:~/code/pool$ ./main
std run:0.00356
xzc@xzc-VirtualBox:~/code/pool$ ./main
std run:0.003925

second times,use cache
my pool run:0.000949
xzc@xzc-VirtualBox:~/code/pool$ ./main
my pool run:0.001046
xzc@xzc-VirtualBox:~/code/pool$ ./main
my pool run:0.001133
xzc@xzc-VirtualBox:~/code/pool$ ./main
my pool run:0.001005
xzc@xzc-VirtualBox:~/code/pool$ ./main
my pool run:0.001074

xzc@xzc-VirtualBox:~/code/pool$ ./main
boost run:2.14777
xzc@xzc-VirtualBox:~/code/pool$ ./main
boost run:2.15328
xzc@xzc-VirtualBox:~/code/pool$ ./main
boost run:2.15201
xzc@xzc-VirtualBox:~/code/pool$ ./main
boost run:2.15536

boost list[3][i] = pool.ordered_malloc( 4 ) 分配失败
*/

void memory_fail()
{
    std::cerr << "no memory anymore !!!" << std::endl;
    exit( 1 );
}

int main()
{
    std::set_new_handler( memory_fail );

    ordered_pool<CHUNK_SIZE> pool;
    char *(list[4][10000]) = {0};

    clock_t start = clock();
    const int max = 1024;
    for ( int i = 0;i < max;i ++ )
    {
        //std::cerr << i << std::endl;
        list[0][i] = pool.ordered_malloc( 1 );
        list[1][i] = pool.ordered_malloc( 2 );
        //list[3][i] = pool.ordered_malloc( 4 );
    }
    for ( int i = 0;i < max;i ++ )
    {
        pool.ordered_free( list[0][i],1 );
        pool.ordered_free( list[1][i],2 );
        //pool.ordered_free( list[3][i],4 );
    }
    std::cout << "my pool run:" << float(clock() - start)/CLOCKS_PER_SEC << std::endl;
/*
    //typedef boost::singleton_pool<char, CHUNK_SIZE> Alloc;
    boost::pool<> Alloc(CHUNK_SIZE);

    clock_t _start = clock();
    for ( int i = 0;i < 10000;i ++ )
    {
        list[0][i] = (char*)Alloc.ordered_malloc( 1 );
        list[1][i] = (char*)Alloc.ordered_malloc( 2 );
        //list[3][i] = (char*)Alloc::ordered_malloc( 4 );
    }
    for ( int i = 0;i < 10000;i ++ )
    {
        Alloc.ordered_free( list[0][i],1 );
        Alloc.ordered_free( list[1][i],2 );
        //Alloc::ordered_free( list[3][i],4 );
    }
    std::cout << "boost run:" << float(clock() - _start)/CLOCKS_PER_SEC << std::endl;
*/
    return 0;
}
