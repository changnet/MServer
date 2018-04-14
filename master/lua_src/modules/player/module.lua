-- module.lua
-- 2018-02-06
-- xzc

-- 玩家子模块基类

local Module = oo.class( nil,... )

function Module:__init( pid,player )
    self.pid = pid
    self.player = player
end

-- 数据存库接口，自动调用
-- 必须返回操作结果
function Module:db_save()
    return true
end

-- 数据加载接口，自动调用
-- 必须返回操作结果
function Module:db_load( sync_db )
    return true
end

-- 数据加载完成自动调用，用于初始化从数据库加载的数据
-- 必须返回操作结果
function Module:db_init()
    return true
end

-- 玩家数据已加载完成，进入场景
-- 必须返回操作结果
function Module:on_login()
    return true
end

-- 玩家退出游戏
-- 必须返回操作结果，但这个结果不影响玩家数据存库
function Module:on_logout()
    return true
end

return Module
