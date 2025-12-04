-- 单个机器人逻辑
Bot = oo.class("Bot")

local AIMgr = require "modules.ai.ai_mgr"
local CsSocket = require "network.cs_socket"

-- 初始化
function Bot:__init(id, ai_id)
    self.id = id
    self.ai = AIMgr.new(self, ai_id)
    self.socket_driver = CsSocket
end

-- 发送数据包
function Bot:send_pkt(cmd, pkt)
    self.s:send_pkt(cmd, pkt)
end

-- 逻辑循环
function Bot:routine()
    print("bot routine", self.id)
end

return Bot
