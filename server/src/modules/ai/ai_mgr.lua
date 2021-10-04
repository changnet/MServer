-- ai管理
local ai_conf = require "config.ai_base_conf"

local AiMgr = oo.singleton(...)

-- 定义AI类型对应的ai逻辑文件
local AI_TYPE = {
    [1] = require "modules.ai.ai_test" -- 测试用的机器人
}

function AiMgr:__init()
end

-- 创建AI逻辑
-- @id:ai_base配置表id
function AiMgr:new(entity, id)
    local conf = ai_conf[id]
    assert(conf, "ai conf id not exist", id)

    local Ai = AI_TYPE[conf.type]
    assert(conf, "ai type not exist", id, conf.type)

    local ai = Ai(entity, conf)
    entity.ai = ai

    return ai
end

local ai_mgr = AiMgr()

return ai_mgr
