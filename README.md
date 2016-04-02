MServer
=========
Mserver是一个基于节点的多进程游戏服务器。每一个节点为一个进程，使用相同的底层(master)来启动进程，通过加载不同的lua脚本实现不同的功能。节点之间通过
tcp进行通信。master使用C++编写，提供了游戏中高性能，高稳定性，脚本不方便完成的组件，包括MySQL、MongoDB、Socket、C++脚本交互、协议序列化、日志等。
MySQL、MongoDB、日志采用了多线程，socket采用了非阻塞epoll，用户可根据自己的习惯继续使用传统的异步回调或者利用lua的coroutine将异步转为同步。通常用
户不需要修改底层，只需要编写lua脚本，即可完成一个高效稳定的游戏服务器，并且可以通过增加节点来提高承载。


编译安装
--------

代码在ubuntu 14.04、debian 7中测试。下面以ubuntu 14.04安装为例:

 * sudo apt-get install libmysqlclient-dev
 * sudo apt-get install lua53
 * sudo apt-get install pkg-config libssl-dev libsasl2-dev
 * install mongo c driver(https://github.com/mongodb/mongo-c-driver/releases)
 * cd MServer/master & make


组件
----

所有组件均提供对应的lua接口，用户只需要在lua调用对应的接口即可使用组件。

 * lua面向对象封装，支持热更，内存监测
 * 重写libev,仅保留io、timer，重写信号处理
 * C++与lua交互封装
 * 非阻塞socket,自定义socket内存池
 * 基于mysql c connector封装mysql，支持lua table直接转换
 * 基于mongo c driver封装mongodb，支持lua table直接转换
 * 基于http-parser的http (client/server)通信模块
 * 基于parson的lua json解析模块
 * 多线程缓冲日志
 * lua_rapidxml，xml解析模块

待实现组件:
-----------

 * protobuf、flatbuffer
 * astar、rsa、zlib、md5、uuid、xml
 * 为lua提供LRU、LFU、优先队列、大小堆等常用数据结构

单个节点master架构
------------------
![节点架构](https://github.com/changnet/MServer/blob/master/picture/master.png)

使用本服务器构建的架构
---------------------

![全服架构](https://github.com/changnet/MServer/blob/master/picture/server%20frame.png)

valgrind测试
-----------

在ubuntu 14.04,debian 7 wheezy上测试通过，但注意以下两点：  
 * mongo c driver在 valgrind 3.7 on Debian Wheezy下mongoc_client_new会引起SIGSEGV，请使用
   3.10以上版本。
 * mongo c driver中的sasl导致很多still reachable内存未释放，见https://github.com/mongodb/mongo-c-driver/blob/master/valgrind.suppressions


#TODO
1. protobuf、platbuffer
2. astar、rsa、zlib、md5、uuid、cjson、xml
3. ps -o 测试缺页中断
4. dump内存情况，包含内存碎片
5. 底层包自动转发机制
6. 关键字过滤(全文检索算法)
7. 寻路算法
8. 测试查询大量结果导致out of memory后线程能否恢复
9. 为lua提供LRU、LFU、优先队列、大小堆等常用数据
#TOSOLVE
1. lsocket不再继承socket，改用组合方式(message_cb需要使用ev_io,recv、send也在lsocket使用)  
   message_cb、listen_cb、connect_cb再做一次模板特化  
   重新封装原始start、stop、send、recv、is_active函数,sending变量  
   ev不会再暴露给lsocket  
   fd由lsocket产生传给socket  
   将bind等函数全封装在socket，因此要重新封装connect,check_connect,listen,accept_one  
2. 测试mysql中NULL指针，空查询结果，存储过程返回是否正确
3. http server/client 压测
