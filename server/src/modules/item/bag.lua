local ItemStore = require("item.item_store")

-- 通用背包类
local Bag = oo.class("Bag", ItemStore)

function Bag:add_res(player, res, op, log_str, ext)
    -- 计算堆叠数量pile
end

function Bag:dec_res(player, res, op, log_str, ext)
end

return Bag
