-- chat.lua
-- 2018-02-06
-- xzc

-- 聊天逻辑

local CHAT = require "modules.chat.chat_header"
local Base_module = require "modules.player.base_module"

local Chat = oo.class( Base_module,... )

function Chat:__init( pid,player )
    Base_module.__init( self,pid,player )
end

return Chat
