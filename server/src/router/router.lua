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
Rpc.proxy_wtype("MongoDB", W_MONGODB)
MongoDB.find_and_modify(...) 这样调用，就无法自动分配到特定节点了，只能部分功能使用
]]

local this = memory("Router", {
    wpolicy = {}, -- worker类型对应的负载均衡策略
    pid_map = {}, -- [pid][wtype] = addr 玩家在对应worker类型的地址的映射
    windex = {}, -- [wtype] = 上一次分配的worker索引
})

local function get_worker_list(wtype)
    -- 可能在起服的时候，就需要用router来发送数据了，因此这个列表不能依赖
    -- 系统是否启动完成，或者worker是否连上
    local wtypes = this.wtypes
    if not wtypes then
        wtypes = {}
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
        this.wtypes = wtypes
    end
    return wtypes[wtype]
end

-- 默认的负载均衡策略（通过轮询依次分配到各个worker，适用于无状态的worker）
local function default_policy(wtype)
    local list = get_worker_list(wtype)
    if not list then
        eprint("no suitable router for worker", wtype)
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

-- 根据worker类型查看玩家所在地址
function Router.find_player_addr(pid, wtype)
end

-- 更新玩家所在worker地址
function Router.update_worker_addr(pid, addr)
end

-- 根据worker类型，查找一个可用worker地址
-- @param ... 用来自定义路由的参数，比如玩家id，副本id等
function Router.find_worker_addr(wtype, ...)
    local func = this.wpolicy[wtype]
    if func then
        return func(wtype, ...)
    else
        return default_policy(wtype)
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

SE.reg(SE_WORKER_READY, on_worker_ready)
SE.reg(SE_WORKER_STOP, on_worker_stop)
