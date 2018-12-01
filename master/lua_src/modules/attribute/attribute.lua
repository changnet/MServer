-- attribute.lua
-- xzc
-- 2018-12-01

-- 战斗属性

local Attribute = oo.class( nil,... )

function Attribute:__init()
    self.attribute = {}
end

-- 添加一个属性
function Attribute:push_one( at,val )
    self.attribute[at] = val + (self.attribute[at] or 0)
end

-- 添加属性列表
function Attribute:push( attribute_list )
    for _,attr in pairs(attribute_list) do
        local at = attr.at
        self.attribute[at] = attr.val + (self.attribute[at] or 0)
    end
end

-- 把另一个属性集合并到当前属性集
function Attribute:merge( other )
    for at,val in pairs( other ) do
        self.attribute[at] = val + (self.attribute[at] or 0)
    end
end

-- 计算当前属性的战力
function Attribute:calc_fight_value()
    -- TODO:这个和配置相关，后面再处理
    return 0
end

return Attribute
