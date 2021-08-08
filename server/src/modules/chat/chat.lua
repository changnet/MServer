-- chat.lua
-- 2018-02-06
-- xzc
-- 聊天逻辑
local channel_func = {}
local CHAT = require "modules.chat.chat_header"
local Module = require "modules.player.module"

local Chat = oo.class(..., Module)

function Chat:__init(pid, player)
    Module.__init(self, pid, player)
end

-- 处理聊天
function Chat:do_chat(conn, pkt)
    local var = self.player:get_misc_var("chat")
    if var.no_chat then -- 被禁言
        return elog("do chat while no_chat:%d", self.pid)
    end

    -- 聊天中带gm
    if GM.chat_gm(self.player, pkt.context) then return end

    local chat_func = channel_func[pkt.channel]
    if not chat_func then
        return elog("channel func not found:%d-%d", self.pid, pkt.channel)
    end

    return chat_func(self, conn, pkt)
end

-- 私聊
function Chat:private_chat(conn, pkt)
    local player = PlayerMgr.get_player(pkt.pid)
    if not player then
        return elog("private chat target player not found:%d", pkt.pid)
    end

    local rpkt = {}
    rpkt.pid = self.pid
    rpkt.name = self.player.name
    rpkt.context = pkt.context
    rpkt.channel = CHAT.CHL_PRIVATE

    player:send_pkt(CHAT.DOCHAT, rpkt)
end

-- 世界聊天
function Chat:world_chat(conn, pkt)
    local rpkt = {}
    rpkt.pid = self.pid
    rpkt.name = self.player.name
    rpkt.context = pkt.context
    rpkt.channel = CHAT.CHL_PRIVATE

    SrvMgr.clt_multicast(2, {}, CHAT.DOCHAT, rpkt)
end

channel_func[CHAT.CHL_WORLD] = Chat.world_chat
channel_func[CHAT.CHL_PRIVATE] = Chat.private_chat

return Chat
