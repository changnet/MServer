-- ai管理
local AiMgr = oo.singleton()

local ai_conf = require "config.ai_base_conf"

-- 定义AI类型对应的ai逻辑文件
local AI_TYPE = {
    [1] = require "modules.ai.ai_test" -- 测试用的机器人
}


-- 创建AI逻辑
-- @param id ai_base配置表id
function AiMgr.new(entity, id)
    local conf = ai_conf[id]
    assert(conf, "ai conf id not exist", id)

    local Ai = AI_TYPE[conf.type]
    assert(conf, "ai type not exist", id, conf.type)

    return Ai(entity, conf)
end

return AiMgr
