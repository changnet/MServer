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
            ELOG( "player db load error,step %d",step )
            return false
        end
    end

    -- db数据初始化，如果模块之间有数据依赖，请自己调整好顺序
    for _,module in pairs( sub_module ) do
        self[module.name]:db_init()
    end

    self:on_login()

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

    self:send_pkt( SC.PLAYER_ENTER,{} )
    PLOG( "player enter,pid = %d",self.pid )

    return true
end

-- 退出游戏
function Player:on_logout()
    g_timer_mgr:del_timer( self.timer )

    for _,module in pairs( sub_module ) do
        self[module.name]:on_logout()
    end

    -- 存库
    for _,module in pairs( sub_module ) do
        self[module.name]:db_save()
    end
end

-- 获取通用的存库数据
function Player:get_misc_var( key )
    return self.misc:get_var( key )
end

return Player
