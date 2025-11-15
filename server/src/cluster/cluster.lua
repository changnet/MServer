-- 集群
Cluster = {
    -- 集群节点类型
    NODE_LOCAL   = 0x00, -- 本地worker，不通过socket通信
    NODE_WORKER  = 0x01, -- worker直连，比process中转快
    NODE_PROCESS = 0x02, -- 通过process转发
    NODE_FORWARD = 0x03, -- 通过另一个进程转发，最慢
}

--[[
## 在配置实现cluster配置，这个配置要保证几点：
1. 同一个服（同一个集群）使用的配置是同一份而不是每个进程单独一份，减少部署难度
2. 进程启动时，需要知道启动什么节点(通过命令行参数确定，如gateway1)
3. 进程启动时，获取集群配置（通过配置文件或者数据库。。。），根据集群配置启动对应的worker

配置示例：
cluster =
{
    gateway1 = {host = "::1", port = 20001},
    gateway2 = {host = "::1", port = 20002},
    db1 = {host = "::1", port = 20003, size = 10}, -- 表示当前进程启动了db1~db10共10个worker
    db11 = {host = "::1", port = 20004},
}
1. 对于小服开服，又需要多个进程的服务器，使用配置应该是比较合适的，因为配置比较固定。
2. 对于真正的集群，可能需要从运维数据库或者通过http读取真正的配置，再按同样的格式构建配置即可
3. 对于动态的集群（需要根据压力动态增删节点），需要自己写一个逻辑定时检测压力启动或关闭节点，或者提供接口给运维手动处理。

## 集群是以一个进程为一个节点，一个进程可以包含多个worker
    例如，进程DB1包含db1-db10这10个worker

## 集群节点之间以名字区分，因此不同进程的名字必须不重复，比如db1，db11

## 两个节点之间一般只有一条tcp链接。这连接一般在主线程发起，然后该进程的所有worker都可以使用
    节点不会自动去连接其他节点。因为需求是不确定的，只用写代码的人知道数据如何互通。全部自动两两连接太浪费

## 通过手动指定名字，允许两个节点之间发起多条tcp链接。但底层只有一个线程处理网络数据，应该提高不了效率。如果
达到单条tcp链接吞吐瓶颈（游戏一般是高cpu低io，并且内网通信质量较高一般是达不到的），一般是开多个节点而不是开多条tcp链接。

## 允许在worker线程与另一个节点之间的链接，只要名字不冲突即可。worker发起的链接消息不需要经过主线程转发，延迟低一些。
但这种情况仅限于消息只由当前worker直接处理，而不需要转发给同进程的其他worker。

## 集群的模式一般分为两种
主从模式：
    有一个主节点(master)管理所有子节点，master节点不可关闭。
点对点模式：
    两个节点之间通过特定的算法直连。比如game1~game10数据全往db1，game11~game20数据全往db2

cluster不提供模式相关的api，需要用户自己实现。

例如，每个子节点都连接到节点1，则可以推举节点1为master：
local MASTER_ID = 1 -- 定义master节点
Cluster.start(node_conf, node) -- 启动当前节点
Cluster.connect("db", 1) -- 连接到master节点
这样所有子节点都连接到master节点，master节点就可以对这些子节点进行管理了

master通常用于调度（负载均衡）。比如场景节点开了10个，一个新玩家进入时，需要让master分配一个压力最小的节点。
后续数据可直接和分配好的节点通信而不需要通过master转发。也可以根据负载的高低增删节点的数量。

## 数据安全
节点之间的直连需要手动。未连接上或者连接已断开发送的消息会报错然后丢弃

集群通过tcp通信,消息默认都是不安全的，网络断开可能会导致消息丢失。玩家的消息一般是可丢弃的（比如场景中的移动请求）
但服务器之间的通信可能很重要，因此重要的数据需要手动使用Durable消息机制发送。

]]

local ClusterWorker = require "cluster.cluster_worker"

local this = memory("Cluster", {
    node    = {}, -- 各节点已认证连接，以node_name为key
    unauth  = {}, -- unauthenticated 未认证连接，以worker对象为key
    -- listen  = cluster_worker, -- 监听的连接
    -- reconnect = {} -- 断线重连的节点
})

-- （如果存在集群配置）启动监听，等待其他节点连接
-- @param node_conf 集群节点配置：{host = "::1", port = 20001}
-- @param node_name 当前节点的名字，不是对端的名字，主要用于日志
function Cluster.listen(node_conf, node_name)
    -- 一个线程只能发起一个监听。即使有多个节点连过来，也只是通过一个监听连接
    assert(not this.listen)

    local worker = ClusterWorker(node_name)
    if not worker:listen(node_conf.ip, node_conf.port) then
        print("cluster listen error", node_conf.ip, node_conf.port)
        return
    end
    printf("cluster listen at %s:%d", node_conf.ip, node_conf.port)

    this.listen = worker
end

-- 关闭所有监听
function Cluster.close_listen()
    local w = this.listen
    if not w then return end

    printf("cluster close listen %s:%d", w.listen_ip, w.listen_port)

    w:close()
    this.listen = nil
end

-- 启动完成后，开启监听
function Cluster.listen_later(cluster_conf, node_name)
    local node_conf = assert(cluster_conf[node_name])
    Startup.reg({
        name = "cluster listen",
        boot = function()
            Cluster.listen(node_conf, node_name)
        end,
        ready = function()
            if this.listen then return true end

            return false, "cluster listen fail"
        end

    }, 0xFFFF)
end

-- 集群模式，启动当前进程的多个节点（每个节点为一个worker）
function Cluster.start_later(cluster_conf, node_name)
    assert(false) -- not implement
end

-- 关闭所有连接
function Cluster.close()
    for node_name, worker in pairs(this.node) do
        printf("cluster close node %s", node_name)
        worker:close()
    end
    this.node = {}

    for _, worker in pairs(this.unauth) do
        local node_name = worker.name
        if not node_name then
            local ip, port = worker:address()
            node_name = string.format("%s:%s", ip, port)
        end
        printf("cluster close unauth node %s", node_name)
        worker:close()
    end
    this.unauth = {}
end

-- 重连断开的节点
local function do_reconnect()
    local rct = this.reconnect or EMPTY
    local count = 0
    for node in pairs(rct) do
        local status = node.status
        if SocketMgr.CLOSED == status then
            count = count + 1
            node:reconnect()
        elseif SocketMgr.OPENED == status then
            rct[node] = nil
        else
            count = count + 1
        end
    end
    if 0 == count then
        Timer.stop(this.rct_timer)
        this.rct_timer = nil
    end
end

-- 把当前节点添加到定时重连队列
local function add_reconnect(node)
    local rct = this.reconnect
    if not rct then
        rct = {}
        this.reconnect = rct
    end

    rct[node] = 1
    if this.rct_timer then return end

    this.rct_timer = Timer.interval(3000, 3000, -1, Rtti.no_hot_fix(do_reconnect))
end

-- 连接到其他集群节点
-- @param node_conf 集群节点配置：{host = "::1", port = 20001}
function Cluster.connect(node_conf, addr)
    local w = WorkerHash[addr]
    if w and not w.cluster_worker then
        assert(false) -- 本地的worker不需要连接
    end

    local node_name = Worker.addr_name(addr)
    local worker = ClusterWorker(node_name)
    if not worker:connect(node_conf.ip, node_conf.port) then
        print("cluster connect error", node_name, node_conf.ip, node_conf.port)
        return
    end
    printf("cluster connect %s %s:%d", node_name, node_conf.ip, node_conf.port)

    this.unauth[worker] = worker
end

-- 连接到其他集群节点
-- @param node_list 集群配置及节点名字列表 {{cluster_conf, node_name}}
function Cluster.connect_later(node_list)
    for _, nl in pairs(node_list) do
        local cluster_conf = nl[1]
        local node_name = nl[2]

        local addr = Worker.name_addr(node_name)
        local node_conf = assert(cluster_conf[node_name])
        Startup.reg({
            name = string.format("cluster connect %s", node_name),
            boot = function()
                Cluster.connect(node_conf, addr)
            end,
            ready = function()
                local d = WorkerData[addr]
                if d and (d.node_type or 0xFFFF) < Cluster.NODE_FORWARD then
                    return true
                end

                return false, string.format("cluster connecting %s", node_name)
            end
        }, 0xFFFF)
    end
end

-- 其他节点连连上
function Cluster.accept(worker)
    this.unauth[worker] = worker

    local ip, port = worker:address()
    printf("cluster accept %s:%d", ip, port)
end

local function set_one_worker_status(src_addr, addr, node_type, status)
    Worker.set_status(src_addr, addr, node_type, status)
    -- 同步该节点的状态到所有的本地worker
    Worker.send_other_local(
        Worker.set_status, src_addr, addr, node_type, status)

    printf("cluster node %s addr = %d, status = %d",
        Worker.addr_name(addr), addr, status)
end

-- 设置集群节点worker状态
-- @param node 负责收发数据的集群节点worker，如果是转发则node为中转的节点
-- @param src_addr 来源地址。A连接B进程，B包含鑫个节点，则这些节点的src_addr都是B进程地址
-- @param addr 集群节点地址
-- @param node_type 节点类型
-- @param status 节点状态
local function set_worker(node, src_addr, addr, node_type, status)
    local data = WorkerData[addr]
    local old = WorkerHash[addr]

    -- 如果存在，则不可能是本地启动的worker
    assert(not old or old.cluster_worker)

    local name = Worker.addr_name(addr)
    if data and node_type >= data.node_type then
        printf("cluster worker %s already exist, addr = %d, old src = %d, new = %d",
            name, addr, old.addr, node.addr)
    end

    WorkerHash[addr] = node

    -- 同步该节点的状态到所有的本地worker
    set_one_worker_status(src_addr, addr, node_type, status)
end

-- 把远程节点的worker添加到worker hash并设置状态数据
function Cluster.set_worker_status(src_addr, addr, node_type, status)
    if not WorkerData[addr] and Worker.STOP ~= status then
        local node = WorkerHash[src_addr]

        assert(node and node.cluster_worker)
        set_worker(node, src_addr, addr, node_type, status)
    else
        set_one_worker_status(src_addr, addr, node_type, status)
    end
    ClusterProxy.response_one(src_addr, addr, status)
end

-- 把集群节点作为一个worker添加到worker hash
local function add_to_worker(node)
    local addr = node.addr
    for _, s in pairs(node.status_list) do
        set_worker(node, addr, s.addr, s.node_type, s.status)
    end

    -- 如果有proxy，同步数据到proxy
    ClusterProxy.response(addr)
    ClusterProxy.request(addr)
end

-- 添加ClusterProxy的代理节点
function Cluster.set_proxy_worker(node, status_list, src_addr)
    if status_list then
        for _, s in pairs(status_list) do
            set_worker(node, src_addr, s.addr, Cluster.NODE_FORWARD, s.status)
        end
    else
        local status = Worker.STOP
        local node_type = Cluster.NODE_FORWARD
        for addr, data in pairs(WorkerData) do
            if data.src_addr == src_addr then
                set_one_worker_status(src_addr, addr, node_type, status)
            end
        end
    end
end

-- 添加ClusterProxy的代理节点
function Cluster.set_one_proxy_worker(node, src_addr, addr, status)
    if Worker.STOP == status then
        set_one_worker_status(src_addr, addr, Cluster.NODE_FORWARD, status)
    else
        set_worker(node, src_addr, addr, Cluster.NODE_FORWARD)
    end
end

-- 处理节点认证结果
function Cluster.authenticate(node, ok)
    assert(node == this.unauth[node])

    local name = node.name

    this.unauth[node] = nil
    if ok then
        this.node[name] = node
        print("cluster node establish", name)
        add_to_worker(node)
    else
        node:close()
        print("cluster node auth fail", name)
    end
end

local function remove_from_worker(node)
    local src_addr = node.addr
    if not WorkerData[src_addr] then return end -- 还没握手成功，没有映射任何worker

    local status = Worker.STOP
    local node_type = Cluster.NODE_WORKER
    for addr, w in pairs(WorkerHash) do
        if w == node then
            set_one_worker_status(src_addr, addr, node_type, status)
        end
    end

    ClusterProxy.response(src_addr)
end

-- 连接断开时，取消认证
function Cluster.unauthenticate(node)
    local name = node.name
    if name then
        this.node[name] = nil
    end

    local socket_stat = node.status

    -- 如果是server端，则直接删除
    -- client端则需要尝试重连
    if node:is_server() then
        this.unauth[node] = nil
    else
        this.unauth[node] = node
        -- 不要直接重连，等定时器定时重连即可
        -- 否则如果是签名等问题连接失败，会不断地循环直连
        -- 主动断开的，不要重连
        if socket_stat == SocketMgr.OPENED
            or socket_stat == SocketMgr.OPENING then
            add_reconnect(node)
        end
    end

    remove_from_worker(node)
    if socket_stat == SocketMgr.OPENING then
        local e, str = node:get_error()
        printf("cluster node %s connect fail(%d): %s", name, e, str)
    else
        print("cluster node disconnect", name)
    end
end

-- 所有节点是否连接完成
function Cluster.ready()
    return table.empty(this.unauth)
end

-- 对所有cluster节点使用Send调用对应的函数
function Cluster.send_all(func, ...)
    for _, w in pairs(this.node) do
        Send.invoke(w.addr, func, ...)
    end
end

-- 对所有cluster节点使用Call调用对应的函数
function Cluster.call_all(func, ...)
    for _, w in pairs(this.node) do
        Call.invoke(w.addr, func, ...)
    end
end

return Cluster
