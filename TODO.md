
* 参考boost实现object_pool，把astar和aoi中的缓存改由object_pool实现，其他系统也看下有没有替换的
* 完成背包、聊天等系统设计
* 客户端进入场景，增加gm系统
* mysql增加lua table直接存mysql接口，并且测试一下和格式化字符串的效率
* 把astar和map模块分开，把map_mgr去掉，都放到lua管理
* 实现场景的static区域属性(不可修改)和dynamic区域属性(可根据逻辑创建和修改)

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
* 统计各副本、各场景在线人数

尝试用upb替换pbc:https://github.com/google/upb
lua_stream中解析proto文件，用proto文件来做schema文件

mem
http://man7.org/linux/man-pages/man3/mallinfo.3.html
