-- base_module.lua
-- 2018-02-06
-- xzc

-- 玩家子模块基类

local Base_module = oo.class( nil,... )

function Base_module:__init__( pid )
    self.pid = pid
end

-- 数据存库接口，自动调用
-- 必须返回操作结果
function Base_module:db_save()
    return true
end

-- 数据加载接口，自动调用
-- 必须返回操作结果
function Base_module:db_load()
    return true
end

-- 数据加载完成自动调用，用于初始化从数据库加载的数据
-- 必须返回操作结果
function Base_module:db_init()
    return true
end

-- 玩家数据已加载完成，进入场景
-- 必须返回操作结果
function Base_module:on_login()
    return true
end

-- 玩家退出游戏
-- 必须返回操作结果，但这个结果不影响玩家数据存库
function Base_module:on_logout()
    return true
end

return Base_module
