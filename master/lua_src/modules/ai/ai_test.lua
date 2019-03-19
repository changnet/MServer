-- 用来测试AI

local AST = require "modules.ai.ai_header"
local loginout = require "modules.ai.action.loginout"

local Ai_test = oo.class( nil,... )

function Ai_test:__init(entity)
    self.entity = entity
end

-- AI逻辑主循环
function Ai_test:routine()
    local state = self.state

    -- 登录、退出
    if state == AST.OFF then
        return loginout:check_and_login(self)
    end

    if state == AST.LOGIN then return end

    if state == AST.ON then
        local is_out = loginout:check_and_logout(self)
        if is_out then return end
    end

    -- 下面这些action都是并行的

    -- 聊天
    chat:check_and_chat(self)
    -- 移动、切换场景
    -- 增加、使用资源
end

return Ai_test
