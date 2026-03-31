-- property.lua
-- xzc
-- 2018-12-07

-- 玩家属性
Property = {}

local S = 1 -- 字符串
local I = 2 -- int
-- local D = 3 -- double（只有客户端不支持int64才会使用double，比如js）

--@class PropertyObj 玩家属性定义
--@field i number 索引(index)，用于和前端通信
--@field t number 类型(type)，字符串1，数字2
--@field c boolean 是否同步到客户端(client)
--@field s boolean 是否存库(save)
--@field f boolean 是否同步到InfoMgr(infomation)
--@field w number|table 同步到哪些worker，如果是同步到多个则是一个table

-- 玩家属性定义(PP = player property)
PP = {
    name  = {i = 1, t = S, c = true, s = true, f = true}, -- 名字
    level = {i = 2, t = I, c = true, s = true, f = true, w = {W.GAME}}, -- 等级
    vip   = {i = 3, t = I, c = true, s = true, f = true}, -- vip等级
}

PP_DEF = {} -- [key] = property 属性定义原始数据
PP_INT_LIST = {} -- {property1, property2, ...} 数字类型属性
PP_STR_LIST = {} -- {property1, property2, ...} 字符串类型属性
PP_SAVE_LIST = {} -- 需要存库的属性
local PP_WORKER_LIST = {} -- [key] = {wtype}需要同步到各个worker的属性

local pairs = pairs
local ipairs = ipairs

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

    local w = p.w
    if w then PP_WORKER_LIST[k] = w end
end

local function comp_property(a, b)
    return PP_DEF[a].i < PP_DEF[b].i
end

local function login_sync(player, data)
    local property = player.property
    for k, wlist in pairs(PP_WORKER_LIST) do
        local v = property[k]
        for _, wtype in ipairs(wlist) do
            local wdata = data[wtype]
            local w_property = wdata.property
            if not w_property then
                w_property = {}
                wdata.property = w_property
            end
            w_property[k] = v
        end
    end
end

-- 更新属性集到其他worker
function Property.update(player, k, v)
    local wlist = PP_WORKER_LIST[k]
    if not wlist then return end

    for _, wtype in ipairs(wlist) do
        PlayerSync.update(player, wtype, v, "property", k)
    end
end

table.sort(PP_INT_LIST, comp_property)
table.sort(PP_STR_LIST, comp_property)

PlayerSync.reg(login_sync)

return PP
