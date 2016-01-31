#MServer

#compile
OpenSSL is required for authentication or for SSL connections to MongoDB. Kerberos or LDAP support requires Cyrus SASL.  

ubuntu 14.04  
    1)sudo apt-get install libmysqlclient-dev  
      The following NEW packages will be installed:  
        libmysqlclient-dev libmysqlclient18 mysql-common  
    2)sudo apt-get install lua53  
    3)sudo apt-get install pkg-config libssl-dev libsasl2-dev  
    4)install mongo c driver(https://github.com/mongodb/mongo-c-driver/releases)  
    5) cd MServer/master & make  


#what had done
1. 重写libev,仅保留io、timer
2. C++与lua交互封装
3. 自定义socket内存池
4. 基于mysql c connector封装mysql lua模块
5. 基于mongo c driver封装mongodb lua模块
6. 基于http-parser的http (client/server)模块

#TODO
5. http(http-parser、libcurl)
6. protobuf、platbuffer
7. astar、rsa、zlib、md5、uuid、cjson、xml
8. ps -o 测试缺页中断
9. dump内存情况，包含内存碎片
10. 底层包自动转发
11. 关键字过滤(全文检索算法)
12. 寻路算法
15. 测试查询大量结果导致out of memory后线程能否恢复
16. 为lua提供LRU、LFU、优先队列、大小堆等常用数据
#TOSOLVE
1. lsocket不再继承socket，改用组合方式
3. 测试mysql中NULL指针，空查询结果，存储过程返回是否正确
