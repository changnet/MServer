-- 一些和装备相关，但又没法放到装备类里的逻辑放这里

local function c_equip_on(player, pkt)
end

local function c_equip_off(player, pkt)
end


NetMsg.reg(M.EquipOn, c_equip_on)
NetMsg.reg(M.EquipOn, c_equip_off)
