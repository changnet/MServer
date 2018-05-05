-- bag.lua
-- 2018-05-05
-- xzc

-- 玩家背包模块

local item_conf = require "config.item_item"

-- 初始化配置
local item_map = {}
for _,v in pairs( item_conf ) do item_map[v.id] = v end

local Module = require "modules.player.module"

local Bag = oo.class( Module,... )

-- 初始化
function Bag:__init( pid,player )
    Module.__init( self,pid,player )

    self.db_query = string.format( '{"_id":%d}',pid )
end


-- 数据存库接口，自动调用
-- 必须返回操作结果
function Bag:db_save()
    g_mongodb:update( "bag",self.db_query,self.root,true )
    return true
end

-- 数据加载接口，自动调用
-- 必须返回操作结果
function Bag:db_load( sync_db )
    local ecode,res = sync_db:find( "bag",self.db_query )
    if 0 ~= ecode then return false end -- 出错
    if not res then return true end -- 新号，空数据
    
    self.root = res[1] or {} -- find函数查不到数据时返回一个空数组

    return true
end

-- 玩家数据已加载完成，进入场景
-- 必须返回操作结果
function Bag:on_login()
    return true
end

-- 玩家退出游戏
-- 必须返回操作结果，但这个结果不影响玩家数据存库
function Bag:on_logout()
    return true
end


return Bag
