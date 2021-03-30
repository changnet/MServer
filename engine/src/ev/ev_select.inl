#include "../global/platform.hpp"
#ifdef __windows__
    #ifdef FD_SETSIZE
        #error "FD_SETSIZE already define"
    #endif
    #define FD_SETSIZE 10240
    #include <winsock2.h>
#else
    #include <sys/select.h>
#endif

#include "ev.hpp"

const char *__BACKEND__ = "select";

/**
 * 为什么使用select?
 * Linux下不用这个的，这个主要是兼容windows下测试用的
 *
 * 为什么不用IOCP？
 * IOCP是启动多个线程，然后给触发事件的fd分配一个线程去读、写，这和当前的事件模型有比较大的区别,
 * 它无法提供各个文件描述符的事件。目前框架用的是Reactor模式，而IOCP其实适合Proactor模式。
 * 参考：https://libevent-users.monkey.narkive.com/XWWaQ6ao/integrate-windows-i-o-completion-port-into-libevent
 *
 * WSAPoll？
 * 这个好像没比select好到哪去，而且其他系统还不通用
 *
 * select的限制
 * https://moythreads.com/wordpress/2009/12/22/select-system-call-limitation/
 * /usr/include/sys/select.h
 * typedef struct  {
 *   long int fds_bits[32];//32bit platform
 * } fd_set;
 * select在linux下只能处理小于1024的文件描述符。不仅仅是能处理的文件描述符数量小于1024，文件
 * 描述符的值本身也不能大于1024，因为1025无法按位放到fds_bits[32]这个结构中。libev提供了一种
 * 绕过这个限制的方式，即不使用fds_bits这个结构，也不使用FD_XXX函数，而是使用模拟这些函数自己创
 * 建一个数组传入到select。这种方式可行，但有一定的风险，毕竟glibc中的fds_bits是固定的
 * 具体见libev中的ev_select.c
 *
 * https://docs.microsoft.com/en-us/windows/win32/winsock/maximum-number-of-sockets-supported-2
 * win下，默认是64，但是可以通过重定义FD_SETSIZE修改
 * The default value in Winsock2.h is 64. If an application is designed to be
 * capable of working with more than 64 sockets using the select and WSAPoll
 * functions, the implementor should define the manifest FD_SETSIZE in every
 * source file before including the Winsock2.h header file
 */

/// backend using select implement
class EVBackend final
{
public:
    EVBackend();
    ~EVBackend();

    void wait(class EV *ev_loop, int64_t timeout);
    void modify(int32_t fd, int32_t old_ev, int32_t new_ev);

private:
    fd_set        *readfds;
};

EVBackend::EVBackend()
{
}

EVBackend::~EVBackend() {}

void EVBackend::wait(class EV *ev_loop, int64_t timeout)
{
}

void EVBackend::modify(int32_t fd, int32_t old_ev, int32_t new_ev)
{
}
