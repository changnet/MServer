local ItemStore = require("item.item_store")

-- 通用装备类
local Equip = oo.class("Equip", ItemStore)

-- 计算属性
function Equip:calc_attr(player)
    self.attrs = {}
end

-- 获取属性
function Equip:get_attr(player)
    return self.attrs
end

-- 检测道具能否穿戴
function Equip:check_takeon(player, item_obj)
end

-- 检测装备能否脱下
function Equip:check_takeoff(player, item_obj)
end

-- 穿戴装备
function Equip:takeon(player, item_obj)
end

-- 脱下装备
function Equip:takeoff(player, item_obj)
end

return Equip
