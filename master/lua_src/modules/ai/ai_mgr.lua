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

-- 创建AI逻辑
-- @id:ai_base配置表id
function Ai_mgr:new( entity,id )
    local conf = ai_conf[id]
    ASSERT( conf,"ai conf id not exist",id )

    local ai = AI_TYPE[conf.type]
    ASSERT( conf,"ai type not exist",id,conf.type )

    return ai(entity,conf)
end

local ai_mgr = Ai_mgr()

return ai_mgr