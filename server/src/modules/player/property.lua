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

PP_DEF = {} -- [key] = property 属性定义原始数据
PP_INT_LIST = {} -- {property1, property2, ...} 数字类型属性
PP_STR_LIST = {} -- {property1, property2, ...} 字符串类型属性
PP_SAVE_LIST = {} -- 需要存库的属性

-- PP转为字符串方便Player.get_property(player, PP.name)这样调用，原始数据放到PP_DEF
for k, p in pairs(PP) do
    PP_DEF[k] = p
    PP[k] = k

    if p.t == I then
        table.insert(PP_INT_LIST, k)
    else
        table.insert(PP_STR_LIST, k)
    end
    if p.s then table.insert(PP_SAVE_LIST, k) end
end

local function comp_property(a, b)
    return PP_DEF[a].i < PP_DEF[b].i
end

table.sort(PP_INT_LIST, comp_property)
table.sort(PP_STR_LIST, comp_property)

return PP
