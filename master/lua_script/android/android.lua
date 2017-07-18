-- android.lua
-- 2015-10-05
-- xzc

-- 机器人对象

local util    = require "util"
local android_mgr = require "android.android_mgr"

local sc = require "command/sc_command"

local SC,CS = sc[1],sc[2]

local network_mgr = network_mgr
local Android = oo.class( nil,... )

-- 构造函数
function Android:__init( index,conn_id )
    self.index   = index
    self.conn_id = conn_id

    g_ai_mgr:load( self,1 ) -- 加载AI
end

-- 发送数据包
function Android:send_pkt( cfg,pkt )
    return network_mgr:send_c2s_packet( self.conn_id,cfg[1],pkt )
end

-- 连接成功
function Android:on_connect()
    -- sid:int;        // 服务器id
    -- time:int;       // 时间戳
    -- plat:int;       // 平台id
    -- sign:string;    // 签名
    -- account:string; // 帐户

    local pkt = 
    {
        sid  = 1,
        time = ev:time(),
        plat = 999,
        account = string.format( "android_%d",self.index )
    }
    pkt.sign = util.md5( LOGIN_KEY,pkt.time,pkt.account )

    self:send_pkt( CS.PLAYER_LOGIN,pkt )
end

-- 断开连接
function Android:on_die()
    PLOG( "android die " .. self.index )
end

-- ping返回
function Android:on_ping( errno,pkt )
    self.ai.on_ping( self )
end

-- 登录返回
function Android:on_login( errno,pkt )
    -- no role,create one now
    if 0 == (pkt.pid or 0) then
        local _pkt = { name = string.format( "android_%d",self.index ) }
        self:send_pkt( CS.PLAYER_CREATE,_pkt )

        return
    end

    self.pid  = pkt.pid
    self.name = pkt.name
    self:enter_world()
end

-- 创角返回
function Android:on_create_role( errno,pkt )
    if 0 ~= errno then
        PLOG( "android_%d unable to create role",self.index )
        return
    end

    self.pid  = pkt.pid
    self.name = pkt.name

    self:enter_world()

    PLOG( "android_%d create role success,pid = %d,name = %s",
        self.index,self.pid,self.name )
end

-- 进入游戏
function Android:enter_world()
    self:send_pkt( CS.PLAYER_ENTER,{} )
end

-- 确认进入游戏完成
function Android:on_enter_world( errno,pkt )
    PLOG( "%s enter world success",self.name )
    g_player_ev:fire_event( PLAYER_EV_ENTER,self )
end

-- 被顶号
function Android:on_login_otherwhere( errno,pkt )
    PLOG( "%s login other where",self.name )
end

return Android
