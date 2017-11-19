#ifndef __DISPATCHER_H__
#define __DISPATCHER_H__

/* 网络数据包分发
 * 1. 分发到其他进程
 * 2. 分发到脚本不同接口
 */

class dispatcher
{
public:
    static void uninstance();
    static class dispatcher *instance();
private:
    dispatcher();
    ~dispatcher();
private:
    lua_State *L;
    static class dispatcher _dispatcher;
};

#endif /* __DISPATCHER_H__ */
