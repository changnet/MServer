-- 处理道具使用
ItemUse = {}

-- 注册道具id使用回调
function ItemUse.reg(id, func)
end

-- 注册道具类型使用回调
function ItemUse.reg_type(item_type, func)
end

local function c_item_use(player, pkt)
end

NetMsg.reg(M.ItemUse, c_item_use)
