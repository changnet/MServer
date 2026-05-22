-- 玩家数据不同worker同步机制
PlayerSync = {}

--[[
1. 哪个worker需要同步玩家对象，需要在定义worker时定义pobj = 1
    同步玩家对象，一般只有该worker需要频繁处理玩家在线时的数据，比如“地图服务器”，需要
    玩家的外显、属性等等
    其他worker比如活动之类的，可以根据pid执行玩家逻辑，一般不需要同步对象

2. property.lua中的属性同步到哪个worker，需要指定w字段
    非property.lua中的数据，通过PlayerSync.reg注册要同步的数据

3. property.lua中中的属性调用set函数时，会自动同步。
    非property.lua中的数据，调用 PlayerSync.update()函数同步
]]

local LOCAL_ADDR = LOCAL_ADDR

local funcs = {}

-- 注册其他模块的同步回调函数
function PlayerSync.reg(func)
    table.insert(funcs, func)
end

-- 登录时同步到其他worker
function PlayerSync.init(player)
    local sync_wtypes = Worker.get_key_type_list("pobj")
    if table.empty(sync_wtypes) then return true end

    local pid = player.pid
    local gaddr = player.gaddr

    -- 先把所有要同步的数据构建到一个table中，再统一同步
    -- 因为有些模块的数据会同时同步到多个worker，不用一个个模块地回调多次
    local data = {}
    for wtype in pairs(sync_wtypes) do
        data[wtype] = {
            pid = pid,
            gaddr = gaddr,
            paddr = LOCAL_ADDR,
            create_time = player.create_time,
            logout_time = player.logout_time,
        }
    end

    for _, func in pairs(funcs) do
        func(player, data)
    end

    for wtype in pairs(sync_wtypes) do
        -- 如果一个worker需要分配节点(比如玩家登录回到上次的场景)，必须在这个函数之前分配
        local addr = Router.find_player_addr(pid, wtype)
        if not addr then
            eprint("player sync init no addr found", pid, wtype)
            return false
        end
        local ok = Call[addr].PlayerSync.on_init(data[wtype])
        if not ok then
            eprint("player sync init fail", pid, wtype)
            return false
        end
    end

    return true
end

-- 登录时同步到其他worker
function PlayerSync.login(player, is_new)
    local sync_wtypes = Worker.get_key_type_list("pobj")
    if table.empty(sync_wtypes) then return true end

    local pid = player.pid
    for wtype in pairs(sync_wtypes) do
        -- 如果一个worker需要分配节点(比如玩家登录回到上次的场景)，必须在这个函数之前分配
        local addr = Router.find_player_addr(pid, wtype)
        local ok = Call[addr].PlayerSync.on_login(pid, is_new)
        if not ok then
            eprint("player sync login fail", pid, wtype)
            return false
        end
    end
end

-- 其他worker收到登录时同步的数据
function PlayerSync.on_init(player)
    local pid = player.pid

    -- 正常不会有玩家，有就是之前逻辑出错，直接顶掉
    local old = PlayerMgr.get_player(pid)
    if old then
        eprint("player sync init player already exist", pid)
    end

    -- 加载数据
    if not PlayerData.load(player) then return false end

    -- 现在还不能设置为在线，因为其他线程模块还可能会加载数据
    -- 如果标记为在线会导致一些模块认为玩家在线从而触发在线逻辑，但取不到数据
    player.status = PlayerStatus.LOGIN
    PlayerMgr.set_unint(pid, player)

    -- 其他worker除了玩家数据还需要存其他数据？后续有需求再说
    -- if not Event.pemit_true(player, EV.LOADING) then
    --     return false
    -- end

    return true
end

-- 非player线程登录逻辑
function PlayerSync.on_login(pid, is_new)
    local player = PlayerMgr.get_uninit_player(pid)
    if not player then
        eprint("player sync login no player found", pid)
        return false
    end

    Router.update_player_comm_addr(pid, player.paddr, player.gaddr)
    Event.pemit(player, EV.LOGIN, is_new)

    player.status = PlayerStatus.NORMAL
    PlayerMgr.set(pid, player)

    return true
end

-- 下线时，其他worker对象也要同步下线
function PlayerSync.logout(player)
    local pid = player.pid
    local sync_wtypes = Worker.get_key_type_list("pobj")

    for wtype in pairs(sync_wtypes) do
        local addr = Router.find_player_addr(pid, wtype)
        local ok = Call[addr].PlayerSync.on_logout(pid)
        if not ok then
            eprint("player sync login fail", pid, wtype)
            return false
        end
    end

    return true
end

-- 其他worker下线
function PlayerSync.on_logout(pid)
    local player = PlayerMgr.get_player(pid)
    if not player then
        eprint("player sync logout no player found", pid)
        return false
    end

    -- 保存数据
    PlayerData.save(player)

    PlayerMgr.set(pid)
    Router.update_player_comm_addr(pid)

     -- 其他worker除了玩家数据还需要存其他数据？后续有需求再说

    return true
end

-- 同步更新其他worker的数据， PlayerSync.update(GAME_ADDR, "property", "name", "abc")
-- @param wtype worker类型
-- @param value 需要更新的数据
-- @param ... 需要更新的key，可以是多层key
function PlayerSync.update(player, wtype, value, ...)
    local pid = player.pid
    local addr = Router.find_player_addr(pid, wtype)
    Send[addr].PlayerSync.on_update(pid, value, ...)
end

-- 其他worker收到数据更新
function PlayerSync.on_update(pid, value, ...)
    local player = PlayerMgr.get_player(pid)
    if not player then
        eprint("player sync update no player found", pid, value, ...)
        return
    end

    table.set_value(player, value, ...)
end
