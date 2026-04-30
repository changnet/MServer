-- 玩家多背包管理
BagMgr = {}

--[[
道具、背包、装备都放到item模块中统一处理

item_store.lua为基类，统一负责数据存取
item.lua负责对外的接口
bag_mgr.lua负责管理多个背包和装备系统

bag.lua为背包类，在此基础上可以扩展出仓库、宝石背包、英雄背包等等
equip.lua为装备系统，穿戴的装备也视为一个背包，穿装备就是把道具放到这个背包
]]

local Bag = require "item.bag"
local Equip = require "item.equip"

local this = memory("BagMgr")
if not this.id_gen then
    this.id_gen = {}
end

-- 生成一个全局唯一的id
function BagMgr.next_id()
end

local mods = {
    {
        id = 1, -- 背包id
        type = Bag, -- 背包类型
        size = 256, -- 背包大小
    },
    {
        id = 2, -- 背包id
        type = Equip, -- 背包类型
        size = 8, -- 8件装备位置
    },
}

-- 根据背包id获取背包对象
function BagMgr.get_store(player, id)
    return player.items[id]
end

-- 加载道具数据
local function on_loading(player, is_new)
    local items = {}
    local pid = player.pid
    for _, mod in ipairs(mods) do
        local id = mod.id
        local obj = mod.type(pid, id, mod.size)
        if not obj:load(is_new) then
            perror(player, "load item fail", id)
            return false
        end

        items[id] = obj
    end

    player.items = items
    return true
end

-- 登录下发数据
local function on_login(player)
    for _, obj in pairs(player.items) do
        obj:send_info(player)
    end
end

-- 定时存数据
local function on_save(player)
    for id, obj in pairs(player.items) do
        if not obj:save(player) then
            perror(player, "save item fail", id)
        end
    end
end

Event.reg(EV.SAVE, on_save)
Event.reg(EV.LOGIN, on_login)
Event.reg(EV.LOADING, on_loading)
