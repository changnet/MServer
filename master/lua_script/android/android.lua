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
function Android:__init( pid )
    self.pid = pid
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
        android_mgr:on_android_kill( self.pid )
        ELOG( "android(%d) connect fail:%s",self.pid,util.what_error(errno) )
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
        sign = "====>>>>md5<<<<====",
        account = string.format( "android_%d",self.pid )
    }

    self:send_pkt( CS.PLAYER_LOGIN,pkt )
end

-- 断开连接
function Android:on_disconnect()
    self.timer:stop()
    self.conn:kill()

    self.timer = nil
    self.conn = nil

    android_mgr:on_android_kill( self.pid )
    PLOG( "android die " .. self.pid )
end

-- 收到数据
function Android:on_command()
    local cmd = self.conn:clt_next()
    while cmd do
        android_mgr:cmd_dispatcher( cmd,self )

        cmd = self.conn:clt_next( cmd )
    end
end

-- 定时器事件
function Android:do_timer()
    self:send_pkt( CS.PLAYER_PING,{dummy = 1} )
    self:send_pkt( CS.PLAYER_PING,{dummy = 1} )
end

-- ping返回
function Android:on_ping( pkt )
end

-- 登录返回
function Android:on_login( errno,pkt )
    vd( pkt )
end

return Android
