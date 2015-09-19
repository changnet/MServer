#include <iostream>
#include <ctime>
#include <boost/pool/pool.hpp>
#include <boost/pool/object_pool.hpp>

using namespace std;
using namespace boost;

const int MAXLENGTH = 100000;

/*
2G ubuntu 14.04 vm

g++ -o test test.cpp -lboost_system
xzc@xzc-VirtualBox:~/code/pool$ ./test
程序运行了 12781 个系统时钟
程序运行了 7431 个系统时钟
xzc@xzc-VirtualBox:~/code/pool$ ./test
程序运行了 14078 个系统时钟
程序运行了 10028 个系统时钟
xzc@xzc-VirtualBox:~/code/pool$ ./test
程序运行了 11624 个系统时钟
程序运行了 7787 个系统时钟
xzc@xzc-VirtualBox:~/code/pool$ ./test
程序运行了 13270 个系统时钟
程序运行了 9534 个系统时钟
xzc@xzc-VirtualBox:~/code/pool$ ./test
程序运行了 14641 个系统时钟
程序运行了 12354 个系统时钟
xzc@xzc-VirtualBox:~/code/pool$ ./test
程序运行了 14127 个系统时钟
程序运行了 11137 个系统时钟
xzc@xzc-VirtualBox:~/code/pool$ ./test
程序运行了 10371 个系统时钟
程序运行了 6878 个系统时钟
*/

int main ( )
{
    boost::pool<> p(sizeof(int));

    int* vec1[MAXLENGTH];
    int* vec2[MAXLENGTH];

    clock_t clock_begin = clock();

    for (int i = 0; i < MAXLENGTH; ++i)
        vec1[i] = static_cast<int*>(p.malloc());

    for (int i = 0; i < MAXLENGTH; ++i)
        p.free(vec1[i]);

    clock_t clock_end = clock();

    cout << "程序运行了 " << clock_end-clock_begin << " 个系统时钟" << endl;

    clock_begin = clock();

    for (int i = 0; i < MAXLENGTH; ++i)
        vec2[i] = new int();

    for (int i = 0; i < MAXLENGTH; ++i)
        delete vec2[i];

    clock_end = clock();

    cout << "程序运行了 " << clock_end-clock_begin << " 个系统时钟" << endl;

    return 0;
}
