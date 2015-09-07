--player.lua
--2015-09-07
--xzc

--玩家对象

local Player = oo.class( nil,"Player" )

--构造函数
function Player:__init( pid )
    self.pid = pid
end

--获取pid
function Player:get_pid()
    return self.pid
end

return Player
