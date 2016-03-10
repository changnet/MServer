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


