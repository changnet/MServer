--player.lua
--2015-09-07
--xzc

--玩家对象

local Player = oo.class( nil,... )

--构造函数
function Player:__init( pid )
    self.pid = pid
end

--获取pid
function Player:get_pid()
    return self.pid
end

function Player:set_conn( conn )
    conn:set_self( self )
    conn:set_read( self.on_read )
    conn:set_disconnected( self.on_disconnect )
    self.conn = conn
end

function Player:on_read( pkt )
    self.conn:raw_send( pkt )
end

function Player:on_disconnect()
    PLOG( "leaving ... " .. self.conn:file_description() )
    self.conn = nil
end

return Player
