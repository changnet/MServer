-- event_header.lua
-- 2018-04-30
-- xzc

-- 玩家事件定义(PLAYER_EVENT PE_XXX)

PE_CONN = 1 -- 与服务器建立连接
PE_ENTER = 2 -- 进入游戏世界
PE_EXIT = 3 -- 离开游戏
PE_DEATH = 4 -- 玩家死亡

-- /////////////////////////////////////////////////////////////////////////////
-- /////////////////////////////////////////////////////////////////////////////
-- /////////////////////////////////////////////////////////////////////////////

-- 系统事件定义

SE_SRV_CONNTED          = 1 -- 其他服务器连接
SE_SRV_DISCONNTED       = 2 -- 其他服务器断开连接
SE_READY                = 3 -- 系统初始化完成
SE_SCRIPT_LOADED        = 4 -- 脚本加载完成(通常用于热更后初始化逻辑，但不能在这里注册事件、协议)
