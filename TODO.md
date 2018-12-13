
* 参考boost实现object_pool，把astar和aoi中的缓存改由object_pool实现，其他系统也看下有没有替换的
* 完成背包、聊天等系统设计
* 客户端进入场景，增加gm系统
* gm增加一个从shell脚本调用http的接口
* mysql增加lua table直接存mysql接口，并且测试一下和格式化字符串的效率
* 在debug模式下，增加个配置可以在打印日志时固定显示进程名，不然日志分不清

#清理test和doc目录中无关的内容，迁移到wiki
#位置同步
http://blog.codingnow.com/2006/04/sync.html  
http://blog.codingnow.com/2012/03/dev_note_12.html

#AOI模块及其算法
http://docs2x.smartfoxserver.com/AdvancedTopics/advanced-mmo-api
http://blog.codingnow.com/2012/03/dev_note_13.html
http://www.cnblogs.com/sniperHW/archive/2012/09/29/2707953.html
http://www.codedump.info/?p=388
https://github.com/xiarendeniao/pomelo-aoi

#Pomelo协议设计参考
https://github.com/NetEase/pomelo/wiki/Pomelo-%E5%8D%8F%E8%AE%AE

#AOI设计
* 算法：十字链表 九宫格 灯塔
* 作用：移动、攻击广播、技能寻敌
* 事件：移动、加入、离开事件，要针对类型处理，如玩家和npc是不一样的
* 异步：事件触发可以弄成异步的，但是目前只考虑同步

#状态监控
* C++内存池统计
* C++主要对象统计
* 脚本在C++中创建的对象统计
* 各协议接收、发送次数、频率统计（包括rpc调用）
* socket流量统计
* 各协议处理耗时统计
* sethook各协议调用函数统计(debug用)
* sethook限制、统计堆栈调用深度，防止死循环(debug用)

https://eli.thegreenplace.net/2013/07/09/library-order-in-static-linking
https://stackoverflow.com/questions/24603142/why-is-operator-new-necessary-when-operator-new-is-enough
https://sourceware.org/binutils/docs/ld/VERSION.html
https://stackoverflow.com/questions/211237/static-variables-initialisation-order
http://blog.fesnel.com/blog/2009/08/25/preloading-with-multiple-symbol-versions/

mem
http://man7.org/linux/man-pages/man3/mallinfo.3.html
