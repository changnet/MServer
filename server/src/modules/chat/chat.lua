-- chat.lua
-- 2018-02-06
-- xzc

-- 聊天模块
local Chat = {
    WORLD   = 1, -- 世界聊天
    PRIVATE = 2, -- 私聊
}

local K = "chat"


local function make_msg(player, pkt)
    pkt.pid = player.pid
    pkt.name = Player.get_property(player, PP.name)
end

-- 私聊
local function private_chat(player, pkt)
    local target = PlayerMgr.get_player(pkt.pid)
    if not target then
        -- TODO 离线
        return print("private chat target player not found", pkt.pid)
    end

    make_msg(player, pkt)

    target:send_pkt(M.ChatMsg, pkt)
end

-- 世界聊天
local function world_chat(player, pkt)
    make_msg(player, pkt)

    -- SrvMgr.clt_multicast(2, {}, CHAT.DOCHAT, rpkt)
end


-- 处理聊天
local function c_chat_msg(player, pkt)
    local var = player:storage(K)
    if var.no_chat then -- 被禁言
        return print("do chat while no_chat", player.pid)
    end

    -- 聊天中带gm
    if GM.chat_gm(player, pkt.context) then return end

    local channel = pkt.channel
    if Chat.WORLD == channel then
        world_chat(player, pkt)
    elseif Chat.PRIVATE == channel then
        private_chat(player, pkt)
    else
        perror(player, "chat msg unknow channel", channel)
        return
    end
end

NetMsg.reg(M.ChatMsg, c_chat_msg)

return Chat
