-- property.lua
-- xzc
-- 2018-12-07

local S = 1 -- 字符串
local I = 2 -- int
-- local D = 3 -- double（只有客户端不支持int64才会使用double，比如js）

-- 玩家属性定义(PP = player property)
PP = {
    -- 名字 = {i = 索引, t = 类型, c = 是否同步客户端, s = 是否存库, sync = 是否同步到玩家基础数据}
    name = {i = 1, t = S, c = true, s = true, sync = true}, -- 名字
    level = {i = 2, t = I, c = true, s = true, sync = true}, -- 等级
    vip = {i = 3, t = I, c = true, s = true, sync = true}, -- vip等级
}

return PP
