-- 玩家和worker路由管理
Router = {}

--[[
关于路由机制

当同一个节点有多个时，数据往哪个节点发送涉及到负载均衡和数据的准确性。比如开启了两个
mysql节点，玩家A的数据第一次发往节点A，第二次发往节点B，那数据就出错了。

同时还要考虑节点的动态增删。这没有通用的方案，只能不同节点按不同策略。

例如：一个无状态的战斗验证worker，通过简单的轮询就可以。但一个负责数据缓存的data节点，
要考虑根据玩家pid分配节点，并保持不变。

这里就会有一个很麻烦的问题，当要获取一个地址时，就需要传入该策略需要的参数，比如玩家pid，
而对于mongodb等节点，可能都没有玩家id而是根据collection来分配的，这就无法统一了。

因此做功能时，就需要根据不同功能传入不同的参数，没法做到统一自动路由。

比如
Rpc.proxy_wtype("MongoDB", W.MONGODB)
MongoDB.find_and_modify(...) 这样调用，就无法自动分配到特定节点了，只能部分功能使用

路由规则
1. 在线玩家是由负载均衡分配，或者玩家自己选（比如选场景分线），必须用find_player_addr

2. 其他用find_worker_addr(wtype, key, value)，key-value有几下组合
    1. key="pid" value=pid 根据玩家分配
    2. key="sid" value=sid 根据服务器分配，用于跨服通信
    3. 无key value 根据轮询分配，这种通常是无状态的worker，或者本来就只有一个节点的worker
    4. 如果传其他值的，必定在set_policy里自定义了路由规则，根据这个值来分配
]]

local this = memory("Router", {
    wpolicy = {}, -- worker类型对应的负载均衡策略
    pid_map = {}, -- [wtype][pid] = addr 玩家在对应worker类型的地址的映射
    windex = {}, -- [wtype] = 上一次分配的worker索引
})

-- 只开启一个worker的类型，直接返回这个worker地址，不需要负载均衡了
local only_one_worker_hash

local PLAYER = W.PLAYER
local GATEWAY = W.GATEWAY
local is_game_srv = g_sharedata:get("server_type") == SRV_TYPE.GAME

local function init_worker_list()
    local wtypes = {}
    for addr, data in pairs(WorkerData) do
        if data.status == Worker.READY then
            local wt = Engine.unmake_address(addr)
            local list = wtypes[wt]
            if not list then
                list = {}
                wtypes[wt] = list
            end
            table.insert(list, {addr = addr})
        end
    end

    return wtypes
end

function Router.get_worker_list(wtype)
    -- 可能在起服的时候，就需要用router来发送数据了，因此这个列表不能依赖
    -- 系统是否启动完成，或者worker是否连上
    local wtypes = this.wtypes
    if not wtypes then
       wtypes = init_worker_list()
        this.wtypes = wtypes
    end
    return wtypes[wtype]
end

-- 默认的负载均衡策略（通过轮询依次分配到各个worker，适用于无状态的worker）
local function default_policy(wtype)
    local list = Router.get_worker_list(wtype)
    if not list then
        eprint("no suitable router for worker, wtype = ", wtype)
        return
    end

    local index = 1 + (this.windex[wtype] or 0)
    local winfo = list[index]
    if not winfo then
        this.windex[wtype] = 1
        winfo = list[1]
        if not winfo then
            eprint("no suitable router for worker", wtype)
            return
        end
    else
        this.windex[wtype] = index
    end

    return winfo.addr
end

local function only_one_worker(wtype)
    -- 不确定哪些worker开多个，多加一个定义比较麻烦
    -- 直接实时判断。如果逻辑未同步pid_map会返回nil出错
    if not only_one_worker_hash then
        only_one_worker_hash = {}
        local list = Router.get_worker_list(wtype)
        if list and #list == 1 then
            only_one_worker_hash[wtype] = list[1].addr
        end
    end

    return only_one_worker_hash[wtype]
end

-- 根据worker类型查看玩家所在地址
function Router.find_player_addr(pid, wtype)
    if is_game_srv then
        -- player scene等多个线程的，需要根据pid找到对应线程地址
        local map = this.pid_map[wtype]
        if map then return map[pid] end

        -- game、data等单线程的，直接返回该类型的地址
        return only_one_worker(wtype)
    else
        error("跨服未实现")
    end
end

-- 更新玩家所在worker地址
-- @param pid number 角色id
-- @param paddr number 玩家所在的player线程地址
-- @param gaddr number 玩家所在的gateway线程地址
function Router.update_player_comm_addr(pid, paddr, gaddr)
    print("update player address", pid, paddr, gaddr)
    local pid_map = this.pid_map
    if paddr then
        table.set_value(pid_map, paddr, PLAYER, pid)
        table.set_value(pid_map, gaddr, GATEWAY, pid)
    else
        table.unset_value(pid_map, PLAYER, pid)
        table.unset_value(pid_map, GATEWAY, pid)
    end
end

-- 更新玩家所在worker地址
-- @param pid number 角色id
-- @param wtype number 线程类型
-- @param addr number 玩家所在的线程地址
function Router.update_player_addr(pid, wtype, addr)
    local pid_map = this.pid_map
    if addr then
        table.set_value(pid_map, addr, wtype, pid)
    else
        table.unset_value(pid_map, wtype, pid)
    end
end

-- 根据worker类型，查找一个可用worker地址
-- @param key string 用来自定义路由的参数类型，比如"pid"、"sid"
-- @param value number|string 用来自定义路由的值，比如玩家id，服务器id等
function Router.find_worker_addr(wtype, key, value)
    if is_game_srv then
        local func = this.wpolicy[wtype]
        if func then
            return func(wtype, key, value)
        else
            return default_policy(wtype)
        end
    else
        error("跨服未实现")
    end
end

-- 设置指定类型worker的负载均衡策略
function Router.set_policy(wtype, func)
    this.wpolicy[wtype] = func
end

local function on_worker_stop(addr)
    local wtypes = this.wtypes
    if not wtypes then return end

    for _, list in pairs(wtypes) do
        for i, data in ipairs(list) do
            if data.addr == addr then
                table.remove(list, i)
                break
            end
        end
    end
end

local function on_worker_ready(addr)
    only_one_worker_hash = nil

    local wtypes = this.wtypes
    if not wtypes then return end

    local wt = Engine.unmake_address(addr)
    local list = wtypes[wt]
    if not list then
        list = {}
        wtypes[wt] = list
    end
    for _, data in ipairs(list) do
        if data.addr == addr then
            eprint("worker ready but already in router", addr)
            return
        end
    end
    table.insert(list, {addr = addr})
end

Event.reg(EV.WORKER_OTHER_READY, on_worker_ready)
Event.reg(EV.WORKER_STOP, on_worker_stop)
