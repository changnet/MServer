-- base.lua
-- 2018-03-23
-- xzc

-- 玩家存库基础数据模块

local Module = require "modules.player.Module"

local Base = oo.class( Module,... )

function Base:__init( pid,player )
    self.root = {}
    Module.__init( self,pid,player )
end


-- 数据存库接口，自动调用
-- 必须返回操作结果
function Module:db_save()
    g_mongodb:update( "base",
        string.format( '{"_id":%d}',self.pid ),self.root,true )
    return true
end

-- 数据加载接口，自动调用
-- 必须返回操作结果
function Module:db_load( sync_db )
    local ecode,res = sync_db:find( 
        "base",string.format( '{"_id":%d}',self.pid ) )
    if 0 ~= ecode then return false end -- 出错
    if not res then return end -- 新号，空数据
    
    self.root = res
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
    self.root.login = ev:time()
    return true
end

-- 玩家退出游戏
-- 必须返回操作结果，但这个结果不影响玩家数据存库
function Module:on_logout()
    self.root.logout = ev:time()
    return true
end

return Base