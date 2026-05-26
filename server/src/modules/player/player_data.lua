-- 玩家通用数据管理
PlayerData = {}

--[[
这里存玩家通用数据，大部分小功能的数据都可以存这里

总容量16M（mongodb的限制），绝大部分项目应该是够用的

比较大的模块（比如背包、英雄数据）等，应该有自己模块的独立存储。放这里会即使不会超也会
导致序列化时卡服务器
]]

__player_memory  = __player_memory or {} -- 玩家内存数据，不存库
__player_storage  = __player_storage or {} -- 玩家存储数据，定时自动存库，按玩家区分

local wname = Worker.type_name(LOCAL_TYPE)

local vals = {}
local opts = {ikey = {"data"}}
local keys = {"pid", 0, "name", wname}

-- 加载玩家通用数据
function PlayerData.load(player)
    local pid = player.pid
    keys[2] = pid

    local e, row = Call[DATA_ADDR].DataCache.get("player_data", keys, nil, opts)
    if 0 ~= e then
        eprint("player data storage load error", pid, e)
        return false
    end

    local s
    local row_data = row.data

    if row_data then
        s = row_data
    else
        assert(Player.is_new(player))
        s = {}
    end

    __player_storage[pid] = s
    __player_memory[pid] = {}

    return true
end

-- 保存玩家通用数据
function PlayerData.save(player)
    local pid = player.pid
    keys[2] = pid

    vals.pid = pid
    vals.name = wname
    vals.data = __player_storage[pid]

    Send[DATA_ADDR].DataCache.update("player_data", keys, vals)
    vals.data = nil

    local size = Rpc.last_codec_size()
    if size > 12 * 1024 * 1024 then
        eprint("player data storage too large", size)
    end
end
