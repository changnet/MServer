-- playe.lua
-- 2017-04-03
-- xzc

-- 针对玩家对象的操作，提供一些通用接口
Player = {}

PlayerStatus = {
    LOGIN   = 1, -- 登录中，正在加载数据
    LOGOUT  = 2, -- 退出中，正在保存数据
    NORMAL  = 4, -- 正常状态
    WLOGOUT = 8 -- 等待退出，等待退出完成
}

__player_memory  = __player_memory or {} -- 玩家内存数据，不存库
__player_storage  = __player_storage or {} -- 玩家存储数据，定时自动存库，按玩家区分

local __player_memory = __player_memory
local __player_storage = __player_storage

-- 如果存在某个模块的存储则返回
--- @param player 玩家对象
--- @param key 模块存储key
--- @param value 模块存储值
function Player.set_storage(player, key, value)
    __player_storage[player.pid][key] = value
end

-- 获取玩家模块存储
--- @param player 玩家对象
--- @param key 模块存储key
function Player.get_storage(player, key)
    return __player_storage[player.pid][key]
end

-- 如果存在内存数据的存储则返回
--- @param player 玩家对象
--- @param key 模块存储key
--- @param value 模块存储值
function Player.set_memory(player, key, value)
    __player_memory[player.pid][key] = value
end

-- 获取玩家内存数据
--- @param player 玩家对象
--- @param key 模块存储key
function Player.get_memory(player, key)
    return __player_memory[player.pid][key]
end

-- 如果存在某个模块的存储则返回
--- @param player 玩家对象
--- @param key 模块存储key
--- @param value 模块存储值
function Player.iset_storage(pid, key, value)
    __player_storage[pid][key] = value
end

-- 获取玩家模块存储
--- @param player 玩家对象
--- @param key 模块存储key
function Player.iget_storage(pid, key)
    return __player_storage[pid][key]
end

-- 如果存在内存数据的存储则返回
--- @param player 玩家对象
--- @param key 模块存储key
--- @param value 模块存储值
function Player.iset_memory(pid, key, value)
    __player_memory[pid][key] = value
end

-- 获取玩家内存数据
--- @param player 玩家对象
--- @param key 模块存储key
function Player.iget_memory(pid, key)
    return __player_memory[pid][key]
end

-- 设置玩家属性
-- @param player 玩家对象
-- @param key 属性key
-- @param value 属性值
function Player.set_property(player, key, value)
    player.property[key] = value
end

-- 获取玩家属性
-- @param player 玩家对象
-- @param key 属性key
function Player.get_property(player, key)
    return player.property[key]
end

-- 登录
function Player.login(player)
    local pid = player.pid
    player.status = PlayerStatus.LOGIN -- 玩家状态，默认登录中

    -- 开始加载玩家数据

    player.money = {} -- 虚拟货币
    player.property = {} -- 属性集合

    -- 在登录过程中下线，标记为等待退出状态，等登录完成后再真正退出
    if player.status & PlayerStatus.WLOGOUT ~= 0 then
        print("player logout in login", pid)
        Player.logout(player, "login_wlogout")
        return
    end

    __player_memory[pid] = {}
    __player_storage[pid] = {}

    player.status = PlayerStatus.NORMAL -- 玩家状态，登录完成
    PlayerMgr.enter_completed(player)
end

-- 退出
function Player.logout(player, why)
    local pid = player.pid
    __player_memory[pid] = nil
    __player_storage[pid] = nil

    PlayerMgr.exit_completed(pid, player.session_id)
end

return Player
