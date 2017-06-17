-- android.lua
-- 2015-10-05
-- xzc

-- 机器人对象

local util    = require "util"
local Timer   = require "Timer"
local android_mgr = require "android.android_mgr"

local sc = require "command/sc_command"

local SC,CS = sc[1],sc[2]

local network_mgr = network_mgr
local Android = oo.class( nil,... )

-- 构造函数
function Android:__init( index,conn_id )
    self.index   = index
    self.conn_id = conn_id
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

-- 定时器事件
function Android:do_timer()
    -- self:send_pkt( CS.PLAYER_PING,{} )
end

-- ping返回
function Android:on_ping( errno,pkt )
    local ts = self.ts or 1

    if ts < 100000 then
        if pkt.time >= ts then
            local max = 1
            self.ts = ts + max
            for i = 1,max do
                self:send_pkt( CS.PLAYER_PING,{} )
            end
        end
    else
        f_tm_stop( "ping done" )
    end
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
    f_tm_start()
    self:send_pkt( CS.PLAYER_PING,{} )
end

-- 被顶号
function Android:on_login_otherwhere( errno,pkt )
    PLOG( "%s login other where",self.name )
end

return Android
