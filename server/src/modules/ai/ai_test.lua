-- 用来测试AI

local AST = require "modules.ai.ai_header"

local test = require "modules.ai.action.test"
local move = require "modules.ai.action.move"
local loginout = require "modules.ai.action.loginout"

local AiTest = oo.class( ... )

function AiTest:__init(entity,conf)
    self.entity = entity
    self.state = AST.OFF
    self.ai_conf = conf
end

-- AI逻辑主循环
function AiTest:routine(ms_now)
    local state = self.state

    -- 登录
    if state == AST.OFF then
        return loginout:check_and_login(self)
    end

    -- 正常登录中，等待登录完成
    if state == AST.LOGIN then return end

    -- 退出
    if state == AST.ON then
        local is_out = loginout:check_and_logout(self)
        if is_out then return end
    end

    -- 下面这些action都是并行的
    -- 聊天
    -- 移动、切换场景
    -- 增加、使用资源

    -- test:gm(self)
    test:chat(self)
    test:ping(self)
    move:random_move(self)
    move:switch_dungeon(self)
end

return AiTest
