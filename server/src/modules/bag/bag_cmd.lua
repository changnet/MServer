-- bag_cmd.lua
-- 2018-05-05
-- xzc
-- 玩家背包 指令
local Bag = require "modules.bag.bag"

Res.reg_module_res("bag", RES_ITEM, Bag.check_item_count, Bag.add_this,
                     Bag.dec)
