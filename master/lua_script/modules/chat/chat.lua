-- chat.lua
-- 2018-02-06
-- xzc

-- 聊天逻辑

local Chat = oo.class( nil,... )

function Chat:__init( pid,player )
    self.pid = pid
end

return Chat
