-- playe.lua
-- 2017-04-03
-- xzc

-- 针对玩家对象的操作，提供一些通用接口
Player = {}

PlayerStatus = {
    LOGIN   = 1, -- 登录中，正在加载数据
    LOGOUT  = 2, -- 退出中，正在保存数据
    NORMAL  = 4, -- 正常状态
    WLOGOUT = 8, -- 已标记退出，但处于加载数据或者保存数据中，等待退出完成
}

require "player.player_data"

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

    return value
end

-- 获取玩家模块存储
--- @param player 玩家对象
--- @param key 模块存储key
function Player.get_storage(player, key)
    return __player_storage[player.pid][key]
end

-- 一些零碎不需要独立key的数据放这里(miscellaneous storage)
function Player.get_misc_stg(player)
    local stg = Player.get_storage(player, "misc")
    if not stg then
        stg = {}
        Player.set_storage(player, "misc", stg)
    end
    return stg
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

-- 设置玩家属性
-- @param player 玩家对象
-- @param key 属性key
-- @param value 属性值
function Player.set_property(player, key, value)
    local old = player.property[key]
    if old == value then return end

    player.property[key] = value
    return Property.update(player, key, value)
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
    local e, row = Call[DATA_ADDR].DataCache.get("player", PLAYER_KEYS)
    if 0 ~= e then
        eprint("player db data error", player.pid, e)
        return false
    end

    -- 缓存没有_id，从数据库加载才有
    if row._id then assert(player.pid == row._id) end

    -- 新号，要初始化一些数据
    local logout_time = row.logout_time
    if not logout_time then
        logout_time = 0
        row.money = {}
    end

    player.property = row.property
    player.money = row.money
    player.create_pfid = row.create_pfid
    player.create_sid = row.create_sid
    player.create_time = row.create_time
    player.logout_time = logout_time -- 上一次退出时间
    player.login_time = Engine.time() -- 本次登录时间

    return true
end

local function load_db_data(player)
    if not load_base_data(player) then return end

    if not PlayerData.load(player) then return end

    -- 其他模块加载数据
    if not Event.pemit_true(player, EV.LOADING) then
        pwarn(player, "load data event error")
        return false
    end

    -- 所有模块加载完成后，同步数据到其他worker（比如场景)
    if not PlayerSync.init(player) then return end

    return true
end

local function init_data(player)
    -- 补全属性集
    local pp = player.property
    for _, k in pairs(PP_INT_LIST) do
        if not pp[k] then pp[k] = 0 end
    end
    for _, k in pairs(PP_STR_LIST) do
        if not pp[k] then pp[k] = 0 end
    end

    Money.init(player)

    -- 基础数据初始化完成，其他模块可以在EV.INIT事件里继续初始化了
    Event.pemit(player, EV.INIT)
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

-- 更新其他线程记录的玩家地址
local function update_addr(player, is_logout)
    local paddr
    local gaddr
    if not is_logout then
        paddr = player.paddr
        gaddr = player.gaddr
    end

    -- 这里用call而不是send，否则后续login事件中其他线程再调用另一个线程，然后获取玩家地址就会取不到

    local pid = player.pid
    local list = Worker.get_key_addr_list("paddr")
    for addr in pairs(list) do
        Call[addr].Router.update_player_comm_addr(pid, paddr, gaddr)
    end
end

-- 登录
function Player.login(player)
    local pid = player.pid

    player.paddr = LOCAL_ADDR
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

    update_addr(player) -- login事件要发协议，必须在此之前设置好addr

    send_base_data(player)

    local is_new = Player.is_new(player)
    Event.pemit(player, EV.LOGIN, is_new)

    PlayerSync.login(player)

    player.status = PlayerStatus.NORMAL -- 玩家状态，登录完成

    -- 登录完成后，才通知前端登录成功，前端才可以请求协议。避免登录过程中用了协程导致
    -- 部分模块未初始化完成就收协议了
    -- 未登录成功，uninit_player中的玩家不接受前端协议
    Send[player.gaddr].Login.enter_game_completed(pid, player.session_id, LOCAL_ADDR)

    return true
end

local function save_base_data(player, is_logout)
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
    data.login_time = player.login_time
    if is_logout then
        data.logout_time = Engine.time()
    else
        data.logout_time = player.login_time
    end

    Send[DATA_ADDR].DataCache.update("player", PLAYER_KEYS, data)
end

local function save_db_data(player, is_logout)
    save_base_data(player, is_logout)
    PlayerData.save(player)

    Event.pemit(player, EV.SAVE)
end

-- 退出
function Player.logout(player, why)
    local pid = player.pid

    Event.pemit(player, EV.LOGOUT)
    save_db_data(player, true)

    PlayerSync.logout(player)

    __player_memory[pid] = nil
    __player_storage[pid] = nil

    update_addr(player, true)
    PlayerMgr.exit_completed(pid, player.session_id)
end

-- 是否是新创建，第一次登录的玩家
function Player.is_new(player)
    return player.logout_time == 0
end

return Player
