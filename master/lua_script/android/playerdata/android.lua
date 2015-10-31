-- android.lua
-- 2015-10-05
-- xzc

-- 机器人对象

--local Client = require "lua.android.net.client"
-- local timer_mgr = require "lua.timer.timermgr"
local Socket = require "Socket"
local Android = oo.class( nil,... )

local words   = { "a","b","c","d","e","f","g","h","i","j","k","l","m","n","o",
                "p","q","r","s","t","u","v","w","x","y","z","0","1","2","3","4",
                "5","6","7","8","9" }

local max_word = 1024*64
local word = {}
local sz = #words
for i = 1,max_word do
    local srand = math.random(sz)
    table.insert( word,words[srand] )
end

local example = table.concat( word )

-- 构造函数
function Android:__init( pid )
    self.pid = pid
end

-- 连接服务器
function Android:born( ip,port )
    local conn = Socket()
    conn:set_self( self )
    conn:set_read( self.talk_msg )
    conn:set_connected( self.alive )
    conn:set_disconnected( self.die )
    
    conn:connect( ip,port,Socket.CLIENT )
    self.conn = conn
end

-- 连接成功
function Android:alive( result )
    if not result then
        ELOG( "android born fail:" .. self.pid )
        return
    end
    
    self:talk()
    --self.timer = timer_mgr:start( 1,self.talk,self )
end

-- 断开连接
function Android:die()
    --timer_mgr:stop( self.timer )
    PLOG( "android die " .. self.pid )
end

-- 收到消息
function Android:talk_msg( pkt )

    if self.last_msg ~= pkt then
        ELOG( "android msg error")
        
        self:die()
        ev:io_kill( self.conn.fd )
        return
    end

    self.last_msg = nil
end

-- 发送消息
function Android:talk()
    local str = string.sub( example,-math.random(max_word) )
    self.last_msg = str
    print( "talk ..." )
    self.conn:raw_send( str )
end

return Android
