-- 实现一个类似于C++ __thiscall 的函数调用机制，即函数的第一个参数为对象
-- 在Lua中，或者叫 self call 更合适？

--[[
    在协议、事件回调中，只能注册一个函数，因为注册回调的时候还没有建立具体的对象。但在
实际回调中，有不少模块是需要对象来调用的，如`Player:get_pid()`。这里利用Lua的反射功能
自动区分回调函数是否需要对象来调用。

    但是这个机制并不完美，只能选择性的实现一些常用的模块，游戏中一般也只有这些模块需要
对象回调，其他模块只能用local函数。毕竟对象回调，那回调的时候就涉及到具体对象的获取，唯
玩家对象可以根据pid统一获取，其他的话无能为力。

    虽然不完美，但至少解决了其他框架中注册回调函数需要传 self，或者在回调函数里手动获取
玩家对象的繁琐。
]]

-- 对象回调
local ThisCall = {}

local this_func = nil

local Player = require "modules.player.player"
local EntityPlayer = require "modules.entity.entity_player"

local PlayerMgr = nil
local EntityMgr = nil

-- 生成特定类的函数hash表
local function make_func()
    -- 目前支持的回调类
    local player_modules = Player:get_sub_modules()

    this_func = {}
    for _, func in pairs(Player) do
        this_func[func] = Player
    end
    for _, func in pairs(EntityPlayer) do
        this_func[func] = EntityPlayer
    end
    for _, m in pairs(player_modules) do
        local name = m.name
        for fname, func in pairs(m.new) do
            this_func[func] = name
        end
    end

    -- 转换成local变量，避免生成的函数去global取
    PlayerMgr = _G.PlayerMgr
    EntityMgr = _G.EntityMgr
end

-- 生成回调函数
-- @param cb 原始回调函数
-- @param this_type 强制指定的回调类型，1 玩家对象， 2玩家实体对象
-- @param msg 出错时用于打印日志的信息
-- @param id 出错时用于打印日志区分是哪个消息出错
-- @return 如果建立了新的回调，则返回该回调函数，否则为nil
function ThisCall.make_from_pid(cb, this_type, msg, id)
    if not this_func then make_func() end

    -- 1. 对于上面指定的类中的函数，不管用Cmd.reg还是Cmd.reg_player、Cmd.reg_entity
    --    注册的，都会自动转成thiscall，即不允许在那些类型用使用 M.func 这类函数
    -- 2. 其他独立的模块，如果需要thiscall回调，则需要显式使用Cmd.reg_player注册
    local m = this_func[cb]
    if not m and not this_type then return end

    if m == Player or 1 == this_type then
        return function(pid, ...)
            local player = PlayerMgr.get_player(pid)
            if not player then
                print("Player callback this not found", pid, msg, id)
                return
            end
            return cb(player, ...)
        end
    elseif m == EntityPlayer or 2 == this_type then
        return function(pid, ...)
            local entity = EntityMgr.get_player(pid)
            if not entity then
                print("EntityPlayer callback this not found", pid, msg, id)
                return
            end
            return cb(entity, ...)
        end
    else
        return function(pid, ...)
            local player = PlayerMgr.get_player(pid)
            if not player then
                print("Player module callback player not found",
                    pid, msg, id)
                return
            end

            return cb(player[m], ...)
        end
    end
end


-- 生成回调函数
-- @param cb 原始回调函数
-- @param this_type 强制指定的回调类型，1 玩家对象， 2玩家实体对象
-- @param msg 出错时用于打印日志的信息
-- @param id 出错时用于打印日志区分是哪个消息出错
-- @return 如果建立了新的回调，则返回该回调函数，否则为nil
function ThisCall.make_from_player(cb, this_type, msg, id)
    if not this_func then make_func() end

    -- 回调的第一个参数为player或者EntityPlayer
    local m = this_func[cb]
    if not m and not this_type then return end

    if m == Player or 1 == this_type then
        return nil
    elseif m == EntityPlayer or 2 == this_type then
        return nil
    else
        return function(player, ...)
            return cb(player[m], ...)
        end
    end
end

return ThisCall
