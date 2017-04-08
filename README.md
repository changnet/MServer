MServer
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
 * lacism，ac算法关键字过滤

待实现组件:
-----------

 * protobuf、flatbuffer
 * astar、rsa、zlib、md5、uuid、xml
 * 为lua提供LRU、LFU、优先队列、大小堆等常用数据结构
 * https_socket(基于openssl(libssl-dev))
 * 利用oo的注册功能实现rsf指令全服热更文件
 * 多因子排序算法(桶排序、插入法排序)

单个节点master架构
------------------
![节点架构](https://github.com/changnet/MServer/blob/master/picture/master.png)

使用本服务器构建的架构
---------------------

![全服架构](https://github.com/changnet/MServer/blob/master/picture/server%20frame.png)

valgrind测试
-----------

在ubuntu 14.04,debian 7 wheezy上测试通过，但注意以下两点：  
 * mongo c driver在 valgrind 3.7 on Debian Wheezy下mongoc_client_new会引起
 SIGSEGV，请使用3.10以上版本。
 * mongo c driver中的sasl导致很多still reachable内存未释放，见
 https://github.com/mongodb/mongo-c-driver/blob/master/valgrind.suppressions

压力测试
-----------
```C++
// ping服务器
struct cping
{
    dummy:int;
}
```
clt --->>> gateway --->>> world --->>> gateway --->>> clt

clt_num  times    sec  tps
1        100000   10   10000

20       100000   42   50000

200      100000   340  58000

#TODO
1. protobuf、flatbuffers
2. astar、rsa、zlib、md5、uuid
3. ps -o 测试缺页中断
4. dump内存情况，包含内存碎片
5. 底层包自动转发机制
6. 关键字过滤(AC算法、KMP算法,模式匹配算法)  
   http://dsqiu.iteye.com/blog/1700312
7. 寻路算法
8. 测试查询大量结果导致out of memory后线程能否恢复
9. 为lua提供LRU、LFU、优先队列、大小堆等常用数据
9. arpg使用状态机来替换各种延时操作，而不要注册各种定时器，不能替换的使用二级定时器
10. https://github.com/CloudFundoo/SSL-TLS-clientserver(polar ssl(mbed tls)实现https)
11. 如果使用coroutine，当前的热更机制是否能更新coroutine中的变量
12. coroutine的应用，参考openrest的cosocket.
https://moonbingbing.gitbooks.io/openresty-best-practices/content/ngx_lua/whats_cosocket.html
13. C++用packet.cpp来处理各种类型的协议发送。分别为packet_flatbuffers.cpp，packet_protobuf.cpp,packet_stream.cpp
转发函数应该是通用的，css_flatbuffers_send这种命名应该改掉
#TOSOLVE
2. 测试mysql中NULL指针，空查询结果，存储过程返回是否正确
3. http server/client 压测
4. 协议分发接口注意不要调用luaL_error，不然会把lua层的while中断
5. buffer的大小BUFFER_MAX客户端、服务器分开限制,recv、send时处理异常
6. ordered_pool增加分配64M服务器大内存接口，指定预分配块数
7. unpack接口在解包数据错误时如何保证缓冲区的正确性
8. 在打包时，注意内存调整时未复制包缓冲区，如果其他数据结构保存过指针，也可能不对
9. pack_node的错误信息能不能更详细
10. 协议注册时，由各个进程在require时发送给router，这时router就可以记录协议号的目标进
程，动态分发，不需要程序员指定。在拆分功能时不需要考虑协议分发
11. socket仅收到协议号时，还未encode或者decode就出错，缓冲区未清会死循环
12. lmongo与lua_bson库合并部分公用api
13. lmysql、lmongo拆分出去，放到deps作为第三方依赖
#rpc
1.调用持久化，参考rabbitMQ
2.可靠调用(有重试机制，对端需要处理重复请求)
3.复杂参数传递(grpc、thrift、wildfly、dubbo),如果不用IDL,只能用bson
4.服务发现(由于是去中心化，节点两两连接，用P2P注册就可以)
5.异步返回(需要为每一次请求分配唯一id，这为实现第2点做准备)
6.同步调用(由于链接也用于服务器的通信，不能阻塞服务器，这点不准备支持)

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
