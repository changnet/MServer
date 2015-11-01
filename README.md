#MServer

#compile
ubuntu 14.04
    1)sudo apt-get install libmysqlclient-dev
      The following NEW packages will be installed:
        libmysqlclient-dev libmysqlclient18 mysql-common
    2)sudo apt-get install lua53
    3) cd MServer/master & make


#what had done
1. rewrite libev core
2. cpp binding with lua

#TODO
1. socket
2. thread
3. callback(socketpair pipe eventfd(best))
4. mysql、mongodb
5. http(http-parser、libcurl)
6. protobuf、platbuffer
7. astar、rsa、zlib、md5、uuid
8. ps -o 测试缺页中断
9. dump内存情况，包含内存碎片
10. 底层包自动转发
11. 关键字过滤(全文检索算法)
12. 寻路算法
13. 将socket、timer从ev分离成独立库，从lua传入ev或者底层做单例instance获取，还是要
    实现socket自杀的情况
14. 主线程要定时判断子线程是否退出(子线程可能做不到任何情况下都通知，可以考虑fd关闭
    消息，需要测试)
15. 测试查询大量结果导致out of memory后线程能否恢复

#TOSOLVE
1. socket内存既是lua管理的，但却在socket_mgr中delete，造成冲突。由此也导致pending_del无效（因此lua可能把它置空）
2. lua_State、leventloop等相互依赖，不好uninstance
3. conected_cb失败时，fd > 0,但却未push到socket_mgr，析构时pop失败
4. C层回调lua层失败出现：fd unreify:please avoid this fd situation,control your watcher
5. lclass push true false 测试
socket 多次listen connect ,在close后取address
timer 多次close start
