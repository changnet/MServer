-- attribute.lua
-- xzc
-- 2018-12-01
-- 战斗属性

local attr_base_conf = require "config.attr_base_conf"

local Attribute = oo.class(...)

function Attribute:__init()
    self.attribute = {}
    -- self.fight_value = 0 -- 当前属性集的战力
end

-- 重置
function Attribute:clear()
    self.attribute = {}
    self.fight_value = 0
end

-- 添加一个属性
-- @at: attribute type,属性类型
function Attribute:push_one(at, val)
    self.attribute[at] = val + (self.attribute[at] or 0)
end

-- 添加属性列表
-- @abt_list：通用属性配置格式{{1,999},{2,9999}}
function Attribute:push(abt_list)
    for _, attr in pairs(abt_list) do
        local at = attr[1]
        self.attribute[at] = attr[2] + (self.attribute[at] or 0)
    end
end

-- 把另一个属性集合并到当前属性集
function Attribute:merge(other)
    for at, val in pairs(other.attribute) do
        self.attribute[at] = val + (self.attribute[at] or 0)
    end
end

-- 计算当前属性的战力
function Attribute:calc_fight_value()
    local fv = 0
    for k, v in pairs(self.attribute) do
        local conf = attr_base_conf[k]
        if conf then
            fv = fv + v * conf.factor
        end
    end

    self.fight_value = fv

    return fv
end

-- 获取缓存的战力(需要调用calc_fight_value后才能取到值)
function Attribute:get_fight_value()
    return self.fight_value
end

return Attribute
