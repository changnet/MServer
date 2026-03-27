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

local PLAYER_KEYS = {"_id", 0}
local PP_INT_LIST = PP_INT_LIST
local PP_STR_LIST = PP_STR_LIST
local PP_SAVE_LIST = PP_SAVE_LIST

local info_pkt = {pp_int = {}, pp_str = {}}

for _ in pairs(PP_INT_LIST) do
    table.insert(info_pkt.pp_int, 0)
end
for _ in pairs(PP_STR_LIST) do
    table.insert(info_pkt.pp_str, "")
end

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

-- 加载玩家基础数据
local function load_base_data(player)
    PLAYER_KEYS[2] = player.pid
    local e, rows = Call[DATA_ADDR].DataCache.get("player", PLAYER_KEYS)
    if 0 ~= e or 1 ~= #rows then
        eprint("player db data error", player.pid, e)
        return false
    end
    --[[
    pp = {name = 1, level = 2, ...}
    money = {},
    ]]
    local data = rows[1]

    assert(player.pid == data._id)

    player.property = data.property
    player.money = data.money
    player.create_pfid = data.create_pfid
    player.create_sid = data.create_sid
    player.create_time = data.create_time
    player.login_time = data.login_time or 0 -- 上一次退出时间

    return true
end

local function load_db_data(player)
    if not load_base_data(player) then return end
end

local function init_data(player)
    local pid = player.pid

    -- 补全属性集
    local pp = player.property
    for _, k in pairs(PP_INT_LIST) do
        if not pp[k] then pp[k] = 0 end
    end
    for _, k in pairs(PP_STR_LIST) do
        if not pp[k] then pp[k] = 0 end
    end

    Money.init(player)

    __player_memory[pid] = {}
    __player_storage[pid] = {}

    -- 基础数据初始化完成，其他模块可以在PE_INIT事件里继续初始化了
    PE.fire_event(player, PE_INIT)
end

local function send_base_data(player)
    local property = player.property

    info_pkt.create_time = player.create_time
    info_pkt.server_time = time.game_time()

    local pp_int = info_pkt.pp_int
    for k, v in pairs(PP_INT_LIST) do
        pp_int[k] = property[v]
    end
    local pp_str = info_pkt.pp_str
    for k, v in pairs(PP_STR_LIST) do
        pp_str[k] = property[v]
    end
    NetMsg.send(player, M.PlayerBase, info_pkt)

    Money.send_info(player)
end

-- 登录
function Player.login(player)
    local pid = player.pid
    player.status = PlayerStatus.LOGIN -- 玩家状态，默认登录中

    -- 加载玩家基础数据
    if not load_db_data(player) then
        return
    end

    -- 在登录过程中下线，标记为等待退出状态，等登录完成后再真正退出
    if player.status & PlayerStatus.WLOGOUT ~= 0 then
        print("player logout in login", pid)
        Player.logout(player, "login_wlogout")
        return
    end

    init_data(player)

    send_base_data(player)
    PE.fire_event(player, PE_LOGIN)

    player.status = PlayerStatus.NORMAL -- 玩家状态，登录完成
    return true
end

local function save_base_data(player)
    local e, rows = Call[DATA_ADDR].DataCache.get("player", PLAYER_KEYS)
    if 0 ~= e or 1 ~= #rows then
        eprint("player db data error", player.pid, e)
        return false
    end

    local property = {}
    local pp = player.property
    for _, key in ipairs(PP_SAVE_LIST) do
        property[key] = pp[key]
    end

    --[[
    pp = {name = 1, level = 2, ...}
    money = {},
    ]]
    local data = {}

    -- 即使一些数据不需要更新，也需要同步到缓存那边，否则下次读取缓存数据就会少了
    data.property = property
    data.money = player.money
    data.create_pfid = player.create_pfid
    data.create_sid = player.create_sid
    data.create_time = player.create_time

    Send[DATA_ADDR].DataCache.update("player", PLAYER_KEYS, data)
end

local function save_db(player)
    save_base_data(player)
end


-- 退出
function Player.logout(player, why)
    local pid = player.pid

    PE.fire_event(player, PE_LOGIN)
    save_db(player)

    __player_memory[pid] = nil
    __player_storage[pid] = nil

    PlayerMgr.exit_completed(pid, player.session_id)
end

-- 是否是新创建，第一次登录的玩家
function Player.is_new(player)
    return player.logout_time == 0
end

return Player
