-- android.lua
-- 2015-10-05
-- xzc

-- 机器人对象

local util    = require "util"
local Timer   = require "Timer"
local Stream_socket  = require "Stream_socket"
local android_mgr = require "android/android_mgr"

local sc = require "command/sc_command"

local SC,CS = sc[1],sc[2]

local Android = oo.class( nil,... )

-- 构造函数
function Android:__init( index )
    self.index = index
end

-- 发送数据包
function Android:send_pkt( cfg,pkt )
    return self.conn:cs_flatbuffers_send( 
            android_mgr.lfb,cfg[1],cfg[2],cfg[3],pkt )
end

-- 连接服务器
function Android:connect( ip,port )
    local conn = Stream_socket()
    conn:set_self_ref( self )
    conn:set_on_command( self.on_command )
    conn:set_on_connection( self.on_connect )
    conn:set_on_disconnect( self.on_disconnect )

    conn:connect( ip,port )
    self.conn = conn

    self.timer = Timer()
    self.timer:set_self( self )
    self.timer:set_callback( self.do_timer )

    self.timer:start( 5,5 )
end

-- 连接成功
function Android:on_connect( errno )
    if 0 ~= errno then
        android_mgr:on_android_kill( self.index )
        ELOG( "android(%d) connect fail:%s",self.index,util.what_error(errno) )
        return
    end

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
function Android:on_disconnect()
    self.timer:stop()
    self.conn:kill()

    self.timer = nil
    self.conn = nil

    android_mgr:on_android_kill( self.index )
    PLOG( "android die " .. self.index )
end

-- 收到数据
function Android:on_command()
    local cmd = self.conn:scmd_next()
    while cmd do
        android_mgr:cmd_dispatcher( cmd,self )

        cmd = self.conn:scmd_next( cmd )
    end
end

-- 定时器事件
function Android:do_timer()
    -- self:send_pkt( CS.PLAYER_PING,{dummy = 1} )
end

-- ping返回
function Android:on_ping( errno,pkt )
    local ts = self.ts or 1

    if ts < 100000 then
        if pkt.time >= ts then
            local max = 1
            self.ts = ts + max
            for i = 1,max do
                self:send_pkt( CS.PLAYER_PING,{dummy = self.ts} )
            end
        end
    else
        f_tm_stop( "ping done" )
    end
end

-- 登录返回
function Android:on_login( errno,pkt )
    -- no role,create one now
    if 0 == pkt.pid then
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
    self:send_pkt( CS.PLAYER_ENTER,{ dummy = 1 } )
end

-- 确认进入游戏完成
function Android:on_enter_world( errno,pkt )
    PLOG( "%s enter world success",self.name )
    f_tm_start()
    self:send_pkt( CS.PLAYER_PING,{dummy = 1} )
end

-- 被顶号
function Android:on_login_otherwhere( errno,pkt )
    PLOG( "%s login other where",self.name )
end

return Android
