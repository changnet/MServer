-- 玩家离线数据管理
PlayerOff = {}

__player_off = __player_off or {}

--[[
1. 需要手动标记变动才会存库（modi_table.lua虽然可以自动标记，但性能太差，暂时不用）

2. 小服可以起服时直接加载所有玩家离线数据。人数太多可以考虑用协程用到再加载
]]

local is_game = (LOCAL_TYPE == W.GAME)

-- 获取玩家离线数据存储，如果不存在返回nil
function PlayerOff.has_storage(pid)
    local s = __player_off[pid]
    if s then return s.storage end
end

-- 获取玩家离线数据存储
function PlayerOff.get_storage(pid)
    local s = __player_off[pid]
    if not s then
        -- 仅允许在game线程调用，这里才能处理全局数据
        if not is_game then error("must use in game") end
        s = {
            storage = {}
        }
        __player_off[pid] = s
    end
    return s.storage
end

-- 获取玩家离线数据存储并标记变动
function PlayerOff.get_modify(pid)
    local s = PlayerOff.get_storage(pid)
    s.modified = true
    return s.storage
end
