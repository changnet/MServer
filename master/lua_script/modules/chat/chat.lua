-- chat.lua
-- 2018-02-06
-- xzc

-- 聊天逻辑

local channel_func = {}
local CHAT = require "modules.chat.chat_header"
local Module = require "modules.player.module"

local Chat = oo.class( Module,... )

function Chat:__init( pid,player )
    Module.__init( self,pid,player )
end

-- 处理聊天
function Chat:do_chat( conn,pkt )
    local var = self.player:get_misc_var( "chat" )
    if var.no_chat then -- 被禁言
        return ELOG( "do chat while no_chat:%d",self.pid )
    end

    local chat_func = channel_func[pkt.channel]
    if not chat_func then
        return ELOG( "channel func not found:%d-%d",self.pid,pkt.channel )
    end

    return chat_func( self,conn,pkt )
end

-- 私聊
function Chat:private_chat( conn,pkt )
    local player = g_player_mgr:get_player( pkt.pid )
    if not player then
        return ELOG( "private chat target player not found:%d",pkt.pid )
    end

    local rpkt = {}
    rpkt.pid = self.pid
    rpkt.name = self.player.name
    rpkt.context = pkt.context
    rpkt.channel = CHAT.CHL_PRIVATE

    player:send_pkt( SC.CHAT_DOCHAT,rpkt )
end

-- 世界聊天
function Chat:world_chat( conn,pkt )
    local rpkt = {}
    rpkt.pid = self.pid
    rpkt.name = self.player.name
    rpkt.context = pkt.context
    rpkt.channel = CHAT.CHL_PRIVATE

    g_network_mgr:clt_multicast( 2,{},SC.CHAT_DOCHAT,rpkt )
end

channel_func[CHAT.CHL_WORLD] = Chat.world_chat
channel_func[CHAT.CHL_PRIVATE] = Chat.private_chat

return Chat
