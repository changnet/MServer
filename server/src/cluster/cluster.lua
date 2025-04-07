-- 集群
Cluster = {name = "cluster"}

--[[
1. 在配置实现cluster配置
这个配置要保证几点：
1. 同一个服（同一个集群）使用的配置是同一份而不是每个进程单独一份，减少部署难度
2. 进程启动时，需要知道启动什么节点(通过命令行参数确定，如gateway1)
3. 进程启动时，获取集群配置（通过配置文件或者数据库。。。），根据集群配置启动对应的worker

cluster =
{
    gateway1 = {host = "::1", port = 20001},
    gateway2 = {host = "::1", port = 20002},
    db1 = {host = "::1", port = 20003, size = 10}, -- 表示当前进程启动了db1~db10共10个worker
    db11 = {host = "::1", port = 20004},
}
对于小服开服，又需要多个进程的服务器，使用配置应该是比较合适的，因为配置比较固定。

对于真正的集群，可能需要从运维数据库或者通过http读取真正的配置，再按同样的格式构建配置即可

对于动态的集群（需要根据压力动态增删节点），需要自己写一个逻辑定时检测压力启动或关闭节点，或者提供接口给运维手动处理。

4. 一个worker启动时，根据配置（来源于配置文件或者数据库或者其他），判断当前是否为集群节点
5. 如果是，则判断是否要开启监听（主从模式的节点并不一定需要监听）
6. 节点不会自动去连接其他节点。因为需求是不确定的，只用写代码的人知道数据如何互通。全部自动两两连接太浪费

7. 模式
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

8. 数据安全
节点之间的直连需要手动。未连接上或者连接已断开发送的消息会报错然后丢弃

集群通过tcp通信,消息默认都是不安全的，网络断开可能会导致消息丢失。玩家的消息一般是可丢弃的（比如场景中的移动请求）
但服务器之间的通信可能很重要，因此重要的数据需要手动使用Durable消息机制发送。

9. 当前的框架上，一个进程可以启动多个worker线程，进程负责调度和转发。进程和集群节点都可以主
连接到远程集群节点。当由进程发起时，则连接是公用的，两个进程间的任意worker都可以相互通信。数据
在线程收到后，再转发给对应的worker。这种模式通常用于多个进程构建单服。

10. 当在一个worker里去连接到远程节点时，这个连接仅仅当前worker可用。远程的数据是直接在当前worker
线程收到而不需要经过转发，效率稍微快点。

11. 当前框架只有一个读写线程发起多条连接也提高不了效率。一般游戏逻辑都是低io高cpu应用，
并且内网通信网络质量也很好，一条链接不应该会成为瓶颈。如果是高io建议启动多个进程而不是在
同一个进程开很多个worker，这样就可以通过多条连接分散压力。
]]

local ClusterWorker = require "cluster.cluster_worker"

local this = global_storage("Cluster", {
    node = {}, -- 各节点已认证连接，以addr为key
    unnode = {}, -- unauthenticated 未认证结点
    listen = {}, -- 监听的连接
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
    printf("cluster %s listen at %s:%d", node_name, conf.ip, conf.port)

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
    if not worker:listen(conf.ip, conf.port) then
        print("cluster connect error", node_name, conf.ip, conf.port)
        return
    end

    this.unnode[addr] = worker
end

-- 节点认证结果
function Cluster.on_authenticate(addr, ok, addr_list)
    local node = this.unnode[addr]
    if not node then
        print("cluster on auth no such node", addr)
        return
    end

    this.unnode[addr] = nil
    if ok then
        this.node[addr] = node
        print("cluster node establish", addr, Worker.addr_name(addr))
    end

    node:close()

    print("cluster on auth fail", addr)
end

-- 连接断开时，取消认证
function Cluster.unauthenticate(addr)
    local node = this.node[addr] or this.unnode[addr]

    this.node[addr] = nil

    -- 如果是server端，则直接删除
    -- client端则需要尝试重连
    if node:is_server() then
        this.unnode[addr] = nil
    else
        this.unnode[addr] = node
        -- 不要直接重连，等定时器定时重连即可
        -- 否则如果是签名等问题连接失败，会不断地循环直连
        -- TODO 主动断开的，不要重连
        -- node:reconnect()
    end

    print("cluster node disconnect", addr, Worker.addr_name(addr))
end

-- 所有节点是否连接完成
function Cluster.ready()
    return table.empty(this.unnode)
end

return Cluster
