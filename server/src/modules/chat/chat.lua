-- chat.lua
-- 2018-02-06
-- xzc

local channel_func = {}

-- 聊天模块
local Chat = {
    CHL_WORLD = 1, -- 世界聊天
    CHL_PRIVATE = 2 -- 私聊
}

local K = "chat"

-- 处理聊天
function Chat.do_chat(player, pkt)
    local var = player:storage(K)
    if var.no_chat then -- 被禁言
        return print("do chat while no_chat", player.pid)
    end

    -- 聊天中带gm
    if GM.chat_gm(player, pkt.context) then return end

    local chat_func = channel_func[pkt.channel]
    if not chat_func then
        return print("channel func not found", player.pid, pkt.channel)
    end

    return chat_func(player, pkt)
end

-- 私聊
function Chat.private_chat(player, pkt)
    local target = PlayerMgr.get_player(pkt.pid)
    if not target then
        return print("private chat target player not found", pkt.pid)
    end

    local rpkt = {}
    rpkt.pid = player.pid
    rpkt.name = player.name
    rpkt.context = pkt.context
    rpkt.channel = CHAT.CHL_PRIVATE

    target:send_pkt(CHAT.DOCHAT, rpkt)
end

-- 世界聊天
function Chat.world_chat(player, pkt)
    local rpkt = {}
    rpkt.pid = player.pid
    rpkt.name = player.name
    rpkt.context = pkt.context
    rpkt.channel = CHAT.CHL_PRIVATE

    SrvMgr.clt_multicast(2, {}, CHAT.DOCHAT, rpkt)
end

channel_func[Chat.CHL_WORLD] = Chat.world_chat
channel_func[Chat.CHL_PRIVATE] = Chat.private_chat

Cmd.reg_player(CHAT.DOCHAT, Chat.do_chat)

return Chat
