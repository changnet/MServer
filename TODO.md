
#优化定时器（加到wiki）
* app定时器，只保留5秒，并且修正为整分钟触发(用real time修正)
* 定时器可配置是否处理time jump
* app一次性delay定时器，
* 玩家一次性delay定时器，
* 副本一次性delay定时器，副本延迟、销毁退出用
* 绝对定时器(absolute timer,指定年月日时分秒运行)
* 相对定时器(relative timer,指定周几，几日，时分秒，按月，按周循环)
* 用小堆实现系统长时定时器，把上面的一些定时器管理起来

## win下epoll接口封装： https://github.com/piscisaureus/wepoll/blob/dist/wepoll.h

