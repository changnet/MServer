-- bag.lua
-- 2018-05-05
-- xzc

-- 玩家背包模块

local item_conf = require "config.item_item"
local level_conf = require "config.player_levelup"

-- 初始化配置
local item_map = {}
for _,v in pairs( item_conf ) do item_map[v.id] = v end

local Module = require "modules.player.module"

local Bag = oo.class( Module,... )

-- 初始化
function Bag:__init( pid,player )
    Module.__init( self,pid,player )

    self.grid = {} -- 格子
    self.db_query = string.format( '{"_id":%d}',pid )
end

-- 获取背包格子数
function Bag:get_max_grid()
    local level = self.player:get_level()

    if not level_conf[level] then return 0 end

    return level_conf[level].bagGrid
end

-- 数据存库接口，自动调用
-- 必须返回操作结果
local db_tbl = {}
function Bag:db_save()
    db_tbl.item = self.grid
    g_mongodb:update( "bag",self.db_query,db_tbl,true )
    return true
end

-- 数据加载接口，自动调用
-- 必须返回操作结果
function Bag:db_load( sync_db )
    local ecode,res = sync_db:find( "bag",self.db_query )
    if 0 ~= ecode then return false end -- 出错

    -- 新号，空数据时res = nil
    if res and res[1] then self.grid = res[1].item end 

    -- 标识为数组，这样在存库时会把各个格子的道具存为数组
    table.set_array( self.grid )

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

-- 添加道具
function Bag:add( id,count )
    local item = { id = id,count = count }

    return self:raw_add( item )
end

-- 添加道具,此函数会复制一份配置
-- item里至少包含id、count，还可以包含其他字段，如绑定、装备强化等级、过期时间...
local exclude_key = { "res" }
function Bag:add_this( item )
    -- 传入的参数通常都是配置，需要复制一份，以免修改到配置
    local new_item = table.copy( item )

    -- 通用的资源配置里，通常会包含一些不必要的字段，需要过滤掉
    for _,key in pairs( exclude_key ) do new_item[key] = nil end

    return self:raw_add( new_item )
end

-- 添加道具
-- 返回未成功添加的数量
-- item里至少包含id、count，还可以包含其他字段，如绑定、装备强化等级、过期时间...
function Bag:raw_add( item )
    local id = item.id
    local count = item.count

    local conf = item_map[id]
    if not conf then
        ELOG( "item conf not found:player = %d,id = %d",self.pid,id )
        return count
    end

    -- 尝试堆叠到旧格子上
    for idx,old_item in pairs( self.grid ) do
        local pile = self:pile_count( old_item,item,count )
        if pile > 0 then
            count = count - pile
            old_item.count = old_item.count + pile
            if count <= 0 then break end
        end
    end

    -- 堆叠完了
    if count <= 0 then return 0 end

    item.count = count -- 修正为堆叠后的数量
    return self:add_to_new_grid( item )
end

-- 插入道具到新格子
function Bag:add_to_new_grid( item )
    local conf = item_map[item.id]
    local max_grid = self:get_max_grid()

    local count = item.count
    for idx = 1,max_grid do
        if not self.grid[idx] then
            local raw_count = math.min( count,conf.pile )
            assert( "item pile conf error",raw_count > 0 )

            local new_item = item
            -- 一个格子装不完，需要复制一份
            if count > raw_count then new_item = table.copy( item ) end

            -- 产生一个唯一的id。这里暂时用字符串表示。如果前端支持64bit，则用int更好
            new_item.uuid = util.uuid_short()

            count = count - raw_count
            self.grid[idx] = new_item
            if count <= 0 then return 0 end
        end
    end

    -- 没有新格子可以插入
    PLOG( "bag full:player = %d,id = %d,count = %d",self.pid,id,count )
    return count
end

-- 能堆叠的数量
local pile_key = { "id" }
function Bag:pile_count( old_item,new_item,new_count )
    -- 指定的属性一样才能堆叠
    for _,key in pairs( pile_key ) do
        if old_item[key] ~= new_item[key] then return 0 end
    end

    local conf = item_map[old_item.id]
    return math.min( conf.pile - old_item.count,new_count )
end

-- 扣除道具
-- 返回未成功扣除的道具，调用此函数之前应该检测道具数量
function Bag:dec( id,count )
    local index = nil

    -- 不要在for循环中修改grid
    for idx in pairs( self.grid ) do
        if id == item.id then index = idx;break end
    end

    if not index then
        ELOG("can NOT dec item,player = %d,id = %d,count = %d",self.pid,id,count)
        return count
    end

    local item = self.grid[index]
    if item.count > count then
        item.count = item.count - count
        return 0
    end

    self.grid[index] = nil
    return self:dec( id,count - item.count )
end

-- 检查道具数量
-- @check_cnt:需要检测的数量，传这个值的话检测到背包中数量>=这个值则返回，否则返回总数量
function Bag:check_item_count( id,check_cnt )
    local count = 0
    for _,item in pairs( self.grid ) do
        if id == item.id then
            count = count + item.count
            if check_cnt and count >= check_cnt then return check_cnt end
        end
    end

    return count
end

-- 空的背包格子数
function Bag:get_empty_grid_count()
    return self:get_max_grid() - table.size( self.grid )
end

return Bag
