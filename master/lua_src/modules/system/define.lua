-- define.lua 服务器全局定义
-- 2017-03-28
-- xzc

-- 服务器索引定义
SRV_NAME =
{
    gateway = 1,
    world   = 2,
    test    = 3,
    example = 4
}

SRV_KEY = "a5c7434a324a6f1c0ef7cb771668695b"
LOGIN_KEY = "409065b7570155637b95e38ca13542e0"

-- 一个玩家最大邮件数量
MAX_MAIL = 200

-- 服务器连接超时设定(秒)
SRV_ALIVE_INTERVAL   = 5
SRV_ALIVE_TIMES      = 5

-- 接入平台
PLATFORM = 
{
    [999] = "test",
}

UNIQUEID =
{
    PLAYER = 1, -- 玩家pid
}

-- 自定义客户端广播方式
CLTCAST = 
{
    PIDS  = 1, -- 这个在底层直接处理
    WORLD = 2, -- 全服广播，但仅限于已Enter World的玩家
    LEVEL = 3, -- 按等级筛选玩家
}
