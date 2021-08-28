-- bag_cmd.lua
-- 2018-05-05
-- xzc
-- 玩家背包 指令
local Bag = require "modules.bag.bag"

Res.reg(RES_ITEM, Bag.check_dec, Bag.check_add, Bag.add, Bag.dec)
