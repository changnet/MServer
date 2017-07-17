MServer(Mini Distributed Game Server)
=========
Mserver是一个基于节点的多进程游戏服务器。每一个节点为一个进程，使用相同的底层(master)
来启动进程，通过加载不同的lua脚本实现不同的功能。节点之间通过tcp进行通信。master使用
C++编写，提供了游戏中高性能，高稳定性，脚本不方便完成的组件，包括MySQL、MongoDB、
Socket、C++脚本交互、协议序列化、日志等。MySQL、MongoDB、日志采用了多线程，socket采用
了非阻塞epoll，用户可根据自己的习惯继续使用传统的异步回调或者利用lua的coroutine将异步
转为同步。通常用户不需要修改底层，只需要编写lua脚本，即可完成一个高效稳定的游戏服务器，
并且可以通过增加节点来提高承载。


编译安装
--------

代码在ubuntu 14.04、debian 7中测试。下面以ubuntu 14.04安装为例:

 * sudo apt-get install libmysqlclient-dev
 * sudo apt-get install lua53
 * sudo apt-get install pkg-config libssl-dev libsasl2-dev
 * sudo apt-get install uuid-dev
 * install mongo c driver(https://github.com/mongodb/mongo-c-driver/releases)
 * download submodule: git submodule init and update
 * cd MServer/master & make
 * update submodule: git submodule update
 * update submodule from origin: git submodule foreach git pull origin master

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
 * lacism,ac算法关键字过滤
 * protobuf、flatbuffer协议
 * md5、uuid等常用算法接口
 * 基于bson的rpc调用

单个节点master架构
------------------
![节点架构](https://github.com/changnet/MServer/blob/master/doc/picture/master.png)

使用本服务器构建的架构
---------------------

![全服架构](https://github.com/changnet/MServer/blob/master/doc/picture/server%20frame.png)

valgrind测试
-----------

在ubuntu 14.04,debian 7 wheezy上测试通过，但注意以下两点：  
 * mongo c driver在 valgrind 3.7 on Debian Wheezy下mongoc_client_new会引起
 SIGSEGV，请使用3.10以上版本。
 * mongo c driver中的sasl导致很多still reachable内存未释放，见
 https://github.com/mongodb/mongo-c-driver/blob/master/valgrind.suppressions

待实现组件:
-----------

* astar、rsa、zlib
* 为lua提供LRU、LFU、优先队列、大小堆等常用数据结构
* https_socket(基于openssl(libssl-dev)https://github.com/CloudFundoo/SSL-TLS-clientserver(polar ssl(mbed tls)实现https))
* 多因子排序算法(桶排序、插入法排序)
* 寻路算法astar
* AOI模块

#待处理
* 如果使用coroutine，当前的热更机制是否能更新coroutine中的变量。底层C++回调脚本如何找到正确的lua_State
* 测试mysql中NULL指针，空查询结果，存储过程返回是否正确
* http server/client 压测
* buffer的大小BUFFER_MAX客户端、服务器分开限制,recv、send时处理异常
* 测试查询大量结果导致out of memory后线程能否恢复
* arpg使用状态机来替换各种延时操作，而不要注册各种定时器，不能替换的使用二级定时器
* ps -o 测试缺页中断
* dump内存情况，包含内存碎片
* 利用oo的注册功能实现rsf指令全服热更文件(协议自动注册的热更)

#位置同步
http://blog.codingnow.com/2006/04/sync.html  
http://blog.codingnow.com/2012/03/dev_note_12.html

#AOI模块及其算法
http://docs2x.smartfoxserver.com/AdvancedTopics/advanced-mmo-api
http://blog.codingnow.com/2012/03/dev_note_13.html
http://www.cnblogs.com/sniperHW/archive/2012/09/29/2707953.html
http://www.codedump.info/?p=388

#Pomelo协议设计参考
https://github.com/NetEase/pomelo/wiki/Pomelo-%E5%8D%8F%E8%AE%AE
