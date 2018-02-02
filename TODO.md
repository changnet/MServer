* 广播、组播支持
1. 玩家广播有两个接口，一个是当前进程广播，一个是到gateway广播。服务器则只有当前进程广播
2. 

network.srv_multicast( conn_list,cmd,pkt )
network.clt_multicast( conn_list,cmd,ecode,pkt )
network.ssc_multicast( mask,pid_list or args_list,cmd,ecode,pkt )

先调用packet打包得到buffer，再用raw_pack循环发送
ssc_multicast有两种组播方式，一种是指定pid数组，另一种是带参数，由网关筛选

* 完成背包、聊天等系统设计
* 客户端进入场景，增加gm系统

#清理test和doc目录中无关的内容，迁移到wiki
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