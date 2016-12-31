-- android.lua
-- 2015-10-05
-- xzc

-- 机器人对象

local Timer   = require "Timer"
local Stream_socket  = require "Stream_socket"
local lua_flatbuffers = require "lua_flatbuffers"
local sc = require "message/sc_message"

local SC,CS = sc[1],sc[2]

local Android = oo.class( nil,... )

-- 构造函数
function Android:__init( pid )
    self.pid = pid
    self.send = 0
    self.recv = 0

    self.lfb = lua_flatbuffers()
    self.lfb:load_bfbs_path( "fbs","bfbs" )
end

-- 连接服务器
function Android:born( ip,port )
    local conn = Stream_socket()
    conn:set_self_ref( self )
    conn:set_on_message( self.talk_msg )
    conn:set_on_connection( self.alive )
    conn:set_on_disconnect( self.die )

    conn:connect( ip,port )
    self.conn = conn
end

-- 连接成功
function Android:alive( result )
    if not result then
        ELOG( "android born fail:" .. self.pid )
        return
    end

    -- local timer = Timer()
    -- timer:set_self( self )
    -- timer:set_callback( self.talk )
    -- timer:start( 0,1 )

    -- self.timer = timer

    local pkt = 
    {
        sign = "====>>>>md5<<<<====",
        account = string.format( "android_%d",self.pid )
    }

    local cfg = CS.PLAYER_LOGIN
    vd( cfg )
    self.conn:cs_flatbuffers_send( self.lfb,cfg[1],cfg[2],cfg[3],pkt )
end

-- 断开连接
function Android:die()
    self.timer:stop()
    self.conn:kill()

    self.timer = nil
    self.conn = nil
    PLOG( "android die " .. self.pid )
end

-- 收到消息
function Android:talk_msg( pkt )
    self.recv = self.recv + 1
    if self.last_msg ~= pkt then
        ELOG( "android msg error,%s\n%s",self.last_msg,pkt )
        self:die()
        return
    end

    self.last_msg = nil
end

-- 发送消息
function Android:talk()

end

return Android
