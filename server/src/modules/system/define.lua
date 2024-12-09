-- define.lua 服务器全局定义
-- 2017-03-28
-- xzc

-- 进程id定义
PROCESS_TEST    = 1 -- 单元测试
PROCESS_GATEWAY = 2 -- 网关
PROCESS_GAME    = 3 -- 游戏逻辑
PROCESS_DATA    = 4 -- 数据读写
PROCESS_CROSS   = 5 -- 跨服
PROCESS_CENTER  = 6 -- 中心服

WORKER_TEST = 1 -- 单元测试

SRV_KEY = "a5c7434a324a6f1c0ef7cb771668695b"
LOGIN_KEY = "409065b7570155637b95e38ca13542e0"

-- 一个玩家最大邮件数量
MAX_MAIL = 200

-- 系统邮件最大数量
MAX_SYS_MAIL = 500

-- 服务器连接超时设定(秒)
SRV_ALIVE_INTERVAL = 5
SRV_ALIVE_TIMES = 5

-- 接入平台
PLATFORM = {[999] = "test"}

UNIQUEID = {
    PLAYER = 1 -- 玩家pid
}

-- 自定义客户端广播方式
CLTCAST = {
    PIDS = 1, -- 按pid广播，这个在C++底层直接处理
    WORLD = 2, -- 全服广播，但仅限于已Enter World的玩家
    LEVEL = 3 -- 按等级筛选玩家
}
