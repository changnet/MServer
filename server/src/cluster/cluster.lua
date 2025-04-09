-- 集群
Cluster = {name = "cluster"}

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

local this = global_storage("Cluster", {
    node    = {}, -- 各节点已认证连接，以node_name为key
    unauth  = {}, -- unauthenticated 未认证连接，以worker对象为key
    listen  = {}, -- 监听的连接
    addr = {}, -- 以addr为key，value为已认证cluster worker
})

-- 把gateway1拆分成gateway和1
local function name_index(node_name)
    -- 注意，node可能是不带数字，不带数字默认为1
    local name, index = string.match(node_name, "(%a+)(%d*)")
    return name, tonumber(index) or 1
end

-- （如果存在集群配置）启动监听，等待其他节点连接
-- @param cluster_conf 集群配置
-- @param node_name 当前节点信息，如"gateway1"
function Cluster.listen(cluster_conf, node_name)
    -- 把gateway1拆分成gateway和1
    local name, index = name_index(node_name)
    local key = string.format("%s%d", name, index)

    local wtype = Worker.name_type(name)
    local addr = Engine.make_address(wtype, index)
    assert(WorkerSetting[addr]) -- 必须是本地worker才能发起监听

    local worker = ClusterWorker(addr)
    local conf = cluster_conf[key]
    if not worker:listen(conf.ip, conf.port) then
        print("cluster listen error", node_name, conf.ip, conf.port)
        return
    end
    printf("cluster listen %s %s:%d", node_name, conf.ip, conf.port)

    this.listen[addr] = worker
end

-- 连接到其他集群节点
-- @param cluster_conf 集群配置
-- @param node_name 当前节点信息，如"gateway1"
function Cluster.connect(cluster_conf, node_name)
    local name, index = name_index(node_name)
    local key = string.format("%s%d", name, index)

    local wtype = Worker.name_type(name)
    local addr = Engine.make_address(wtype, index)
    assert(not WorkerSetting[addr]) -- 本地的worker不需要连接

    local worker = ClusterWorker(addr)
    local conf = cluster_conf[key]
    if not worker:connect(conf.ip, conf.port) then
        print("cluster connect error", node_name, conf.ip, conf.port)
        return
    end
    printf("cluster connect %s %s:%d", node_name, conf.ip, conf.port)

    this.unauth[worker] = worker
end

-- 其他节点连连上
function Cluster.accept(worker)
    this.unauth[worker] = worker

    local ip, port = worker:address()
    printf("cluster accept %s:%d", ip, port)
end

-- 处理节点认证结果
function Cluster.authenticate(node, ok, addr_list)
    assert(node == this.unauth[node])

    local name = node.name

    this.unauth[node] = nil
    if ok then
        this.node[name] = node
        print("cluster node establish", name)
    else
        node:close()
        print("cluster node auth fail", name)
    end
end

-- 连接断开时，取消认证
function Cluster.unauthenticate(worker)
    local name = worker.name
    if name then
        this.node[name] = nil
    end

    -- 如果是server端，则直接删除
    -- client端则需要尝试重连
    if worker:is_server() then
        this.unauth[worker] = nil
    else
        this.unauth[worker] = worker
        -- 不要直接重连，等定时器定时重连即可
        -- 否则如果是签名等问题连接失败，会不断地循环直连
        -- TODO 主动断开的，不要重连
        -- node:reconnect()
    end

    if worker.status == SocketMgr.OPENING then
        local e, str = worker:get_error()
        printf("cluster node %s connect fail(%d): %s", name, e, str)
    else
        print("cluster node disconnect", name)
    end
end

-- 所有节点是否连接完成
function Cluster.ready()
    return table.empty(this.unauth)
end

return Cluster
