-- define.lua 服务器全局定义
-- 2017-03-28
-- xzc

-- 这个文件采用特殊的加载方式，热更时会重新赋值，只能在这里定义常量

-- 服务器类型定义
APP = {}
DEFINE_BEG(APP)
GATEWAY = 0x1 -- 网关
WORLD   = 0x2 -- 世界服
AREA    = 0x3 -- 场景区域服务器
TEST    = 0x9 -- 执行测试用例
DEFINE_END(APP)

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
    PIDS = 1, -- 这个在底层直接处理
    WORLD = 2, -- 全服广播，但仅限于已Enter World的玩家
    LEVEL = 3 -- 按等级筛选玩家
}
