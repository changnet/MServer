-- playe.lua
-- 2017-04-03
-- xzc

-- 玩家对象

local g_network_mgr = g_network_mgr
local Auto_id = require "modules.system.auto_id"

--[[
玩家子模块，以下功能会自动触发
1. 创建对象
2. 加载数据库:db_load
3. 数据初始化:db_init
4. 定时存库  :db_save
5. 定时器    :on_timer
]]

local Base = require "modules.player.base"
local Chat = require "modules.chat.chat"
local Misc = require "modules.misc.misc"
local Bag  = require "modules.bag.bag"

local sub_module =
{
    { name = "base",new = Base },
    { name = "chat",new = Chat },
    { name = "misc",new = Misc },
    { name = "bag" ,new = Bag  },
}

local Player = oo.class( nil,... )

function Player:__init( pid )
    self.pid = pid

    self.auto_id = Auto_id()

    self.timer_cnt = 0
    self.timer_1scb = {}
    self.timer_5scb = {}

    -- 创建各个子模块，这些模块包含统一的db存库、加载、初始化规则
    for _,module in pairs( sub_module ) do
        self[module.name] = module.new( pid,self )
    end

    self.timer = g_timer_mgr:new_timer( self,1,1 )
    g_timer_mgr:start_timer( self.timer )
end

-- 获取玩家id
function Player:get_pid()
    return self.pid
end

-- 发送数据包到客户端
function Player:send_pkt( cmd,pkt,ecode )
    local srv_conn = g_network_mgr:get_gateway_conn()

    return srv_conn:send_clt_pkt( self.pid,cmd,pkt,ecode )
end

-- 定时器事件
function Player:do_timer()
    -- TODO: 是否有必要启动两个定时器，在没有回调时停止对应的定时器
    self.timer_cnt = self.timer_cnt + 1
    for _,cb in pairs( self.timer_1scb ) do cb() end

    if 0 == self.timer_cnt%5 then
        for _,cb in pairs( self.timer_5scb ) do cb() end
    end
end

-- 注册1s定时器
function Player:register_1stimer( callback )
    local id = self.auto_id:next_id()

    self.timer_1scb[id] = callback
    return id
end

-- 取消1s定时器
function Player:remove_1stimer( id )
    self.timer_1scb[id] = nil
end

-- 注册1s定时器
function Player:register_5stimer( callback )
    local id = self.auto_id:next_id()

    self.timer_5scb[id] = callback
    return id
end

-- 取消1s定时器
function Player:remove_5stimer( id )
    self.timer_5scb[id] = nil
end

-- 开始从db加载各模块数据
function Player:module_db_load( sync_db )
    -- 根据顺序加载数据库数据
    for step,module in pairs( sub_module ) do
        local ok = self[module.name]:db_load( sync_db )
        if not ok then
            ELOG( "player db load error,pid %d,step %d",self.pid,step )
            return false
        end
    end

    -- db数据初始化，如果模块之间有数据依赖，请自己调整好顺序
    for _,module in pairs( sub_module ) do
        self[module.name]:db_init()
    end

    self:on_login()
    self.base_root = self.base.root -- 增加一个引用，快速取基础数据

    return true
end

-- 开始加载数据
function Player:db_load()
    local sync_db = g_mongodb:new_sync( self.module_db_load,self )

    return sync_db:start( self,sync_db )
end

-- 登录游戏
function Player:on_login()
    for _,module in pairs( sub_module ) do
        self[module.name]:on_login()
    end


    g_player_ev:fire_event( PLAYER_EV.ENTER,self )
    g_log_mgr:login_or_logout( self.pid,LOG.LOGIN )

    self:send_pkt( SC.PLAYER_ENTER,{} )
    PRINTF( "player enter,pid = %d",self.pid )

    return true
end

-- 退出游戏
function Player:on_logout()
    g_timer_mgr:del_timer( self.timer )

    -- 退出事件
    g_player_ev:fire_event( PLAYER_EV.EXIT,self )

    -- 各个子模块退出处理
    for _,module in pairs( sub_module ) do
        self[module.name]:on_logout()
    end

    -- 存库
    for _,module in pairs( sub_module ) do
        self[module.name]:db_save()
    end
    g_log_mgr:login_or_logout( self.pid,LOG.LOGOUT )

    PRINTF( "player logout,pid = %d",self.pid )
end

-- 获取对应的模块
-- @name:模块名，参考sub_module
function Player:get_module( name )
    return self[name]
end

-- 获取通用的存库数据
function Player:get_misc_var( key )
    return self.misc:get_var( key )
end

-- 获取元宝数量
function Player:get_gold()
    return self.base_root.gold
end

-- 增加元宝
function Player:add_gold( count,op,sub_op )
    self.base_root.gold = self.base_root.gold + count
end

-- 增加元宝
function Player:dec_gold( count,op,sub_op )
    self.base_root.gold = self.base_root.gold - count
end

-- 获取等级
function Player:get_level()
    return self.base_root.level
end

return Player
