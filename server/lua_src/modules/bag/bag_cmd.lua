-- bag_cmd.lua
-- 2018-05-05
-- xzc

-- 玩家背包 指令

local Bag = require "modules.bag.bag"

g_res:reg_module_res( "bag",RES.ITEM,Bag.check_item_count,Bag.add_this,Bag.dec )