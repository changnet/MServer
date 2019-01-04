-- base.lua
-- 2018-03-23
-- xzc

-- 玩家存库基础数据模块

local level_conf = require_conf("player_levelup")

local Module = require "modules.player.module"

local Base = oo.class( Module,... )

function Base:__init( pid,player )
    self.root = {}
    Module.__init( self,pid,player )
end


-- 数据存库接口，自动调用
-- 必须返回操作结果
function Base:db_save()
    g_mongodb:update( "base",
        string.format( '{"_id":%d}',self.pid ),self.root,true )
    return true
end

-- 数据加载接口，自动调用
-- 必须返回操作结果
function Base:db_load( sync_db )
    local ecode,res =
        sync_db:find( "base",string.format( '{"_id":%d}',self.pid ) )
    if 0 ~= ecode then return false end -- 出错
    if not res then return end -- 新号，空数据

    self.root = res[1] -- find函数返回的是一个数组

    return true
end

-- 数据加载完成自动调用，用于初始化从数据库加载的数据
-- 必须返回操作结果
function Base:db_init()
    -- 新玩家初始化
    if 1 == self.root.new then
        self.root.level = 1 -- 初始1级
        self.root.gold  = 0
    end

    -- 计算基础属性
    self:calc_abt()

    return true
end

-- 计算基础属性
function Base:calc_abt()
    local conf = level_conf[self.root.level]
    self.player:set_sys_abt( ABTSYS.BASE,conf.attr )
end

-- 玩家数据已加载完成，进入场景
-- 必须返回操作结果
local base_info = {}
function Base:on_login()
    self.root.login = ev:time()

    -- 更新基础数据到场景服
    base_info.sex = self.root.sex
    base_info.level = self.root.level
    base_info.name = self.root.name

    g_rpc:invoke("player_update_base",self.pid,base_info,true)
    return true
end

-- 玩家退出游戏
-- 必须返回操作结果，但这个结果不影响玩家数据存库
function Base:on_logout()
    self.root.logout = ev:time()
    return true
end

-- 发送基础数据
function Base:send_info()
    local pkt = {}
    pkt.gold = self.root.gold or 0

    self.player:send_pkt( SC.PLAYER_BASE,pkt )
end

-- 更新虚拟资源(铜钱、元宝等)
local res_pkt = { res_type = 0,val = 0 }
function Base:update_res( res_type,val )
    res_pkt.val      = val
    res_pkt.res_type = res_type
    self.player:send_pkt( SC.PLAYER_UPDATE_RES,res_pkt )
end

return Base
