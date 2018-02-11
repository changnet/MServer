-- define.lua 服务器全局定义
-- 2017-03-28
-- xzc

-- >>>>>>>>>>>>> !!!此文件定义的内容不要本地化，否则不能热更!!! <<<<<<<<<<<<<<<<<< 

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

-- 服务器连接超时设定
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

return { ["SRV_NAME"] = SRV_NAME }