-- playe.lua
-- 2017-04-03
-- xzc

-- 玩家对象

local g_network_mgr = g_network_mgr

local gateway_session = g_unique_id:srv_session( 
    "gateway",tonumber(Main.srvindex),tonumber(Main.srvid) )

-- local Bag = require "item.bag"

--[[
玩家子模块，以下功能会自动触发
1. 创建对象
2. 加载数据库:db_load
3. 数据初始化:db_init
4. 定时存库  :db_save
5. 定时器    :on_timer
]]

local Chat = require "modules.chat.chat"
local Misc = require "modules.misc.misc"

local sub_module = 
{
    { name = "chat",new = Chat },
    { name = "misc",new = Misc },
}

local Player = oo.class( nil,... )

function Player:__init( pid )
    self.pid = pid

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

-- 发送数据包到客户端
function Player:send_pkt( cfg,pkt )
    local srv_conn = g_network_mgr:get_srv_conn( gateway_session )

    return srv_conn:send_clt_pkt( self.pid,cfg,pkt )
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
    table.insert( self.timer_1scb )
end

-- 取消1s定时器
function Player:remove_1stimer()
end

-- 开始从db加载各模块数据
function Player:module_db_load()
    self.db_step = 0
    self.load_step = 0

    -- 根据顺序加载数据库数据
    for _,module in pairs( sub_module ) do
        self[module.name].db_load()
        self.db_step = self.db_step + 1
    end

    -- db数据初始化，如果模块之间有数据依赖，请自己调整好顺序
    for _,module in pairs( sub_module ) do
        self[module.name].db_init()
    end
end

-- 登录游戏
function Player:on_login()
    for _,module in pairs( sub_module ) do
        self[module.name].on_login()
    end
end

-- 退出游戏
function Player:on_logout()
    for _,module in pairs( sub_module ) do
        self[module.name].on_logout()
    end
end

return Player