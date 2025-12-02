-- 单个机器人逻辑
Bot = oo.class("Bot")

local AIMgr = require "modules.ai.ai_mgr"
local CsSocket = require "network.cs_socket"

-- 初始化
function Bot:__init(id, ai_id)
    self.id = id
    self.ai = AIMgr.new(self, ai_id)
end

-- 逻辑循环
function Bot:routine()
    print("bot routine", self.id)
end

return Bot
