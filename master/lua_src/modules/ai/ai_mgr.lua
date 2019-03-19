-- ai管理

local ai_conf = require_conf "ai_base"

local Ai_mgr = oo.singleton( nil,... )

-- 定义AI类型对应的ai逻辑文件
local AI_TYPE =
{
    [1] = require "modules.ai.ai_test", -- 测试用的机器人
}

function Ai_mgr:__init()
end

function Ai_mgr:new( entity,ai_type )
    local ai = AI_TYPE[ai_type]

    return ai(entity)
end

local ai_mgr = Ai_mgr()

return ai_mgr