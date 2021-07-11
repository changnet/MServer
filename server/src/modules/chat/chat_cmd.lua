-- chat_cmd.lua
-- 2018-02-06
-- xzc
-- 聊天入口文件
local Chat = require "modules.chat.chat"

local function chat_clt_cb(cmd, cb_func)
    local cb = function(conn, pid, pkt)
        local player = g_player_mgr:get_player(pid)
        if not player then
            return ERROR("chat call back no player found:%d", pid)
        end
        return cb_func(player.chat, conn, pkt)
    end

    Cmd.reg(cmd, cb)
end

chat_clt_cb(CHAT.DOCHAT, Chat.do_chat)
