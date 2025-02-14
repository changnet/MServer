-- 集群
local Cluster = {}

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

9. 一个worker的地址是对当前进程所有worker都可见。当某一个节点连接另一个节点B时，当前进
程的所有worker都没有必要再次发起一次连接了。因为当前框架只有一个读写线程发起多条连接也提
高不了效率。一般游戏逻辑都是低io高cpu应用，并且内网通信网络质量也很好，一条链接不应该会
成为瓶颈。如果是高io建议启动多个进程而不是在同一个进程开很多个worker，这样就可以通过多条
连接分散压力。

10. 如果当前进程的多个集群节点都需要与远程的节点通信，一般由进程发起连接。其他worker把数据
发给进程，再由进程通过socket发给远程节点。但如果有需要，也可以在一个worker里发起连接，由这
个worker负责把数据转发给远程节点。
]]

-- 启动当前进程的集群节点
local function start_node(conf)
end

-- （如果存在集群配置）启动集群
-- @param conf 集群配置
-- @param node 当前节点信息，如"gateway1"
function Cluster.start(node_conf, node)
    if not node_conf then return end

    -- 把gateway1拆分成gateway和1
    local name, index = string.match(node, "(%a+)(%d*)")
    local key = name
    if not index then
        index = 1 -- 未指定时，默认为1
        key = string.format("%s1", name)
    end

    local conf = node_conf[key]
    -- 当前进程可能不需要启动集群节点
    if conf then
        start_node(conf, index)
    end
end

-- 连接集群节点
-- @param name 集群名字，如gateway
-- @param from 节点开始的索引
-- @param to 节点结束的索引
function Cluster.connect(name, from, to)
    -- 如果是本地worker，不要发起链接
    -- 尚未启动的节点连接不到如何处理
    -- 动态增减的节点如何处理
    -- ClusterWorker是否添加到C++底层，允许C++发送消息给远程节点（好像没必要）

    -- 同一个进程中，一个worker(包括ClusterWorker)的地址是对所有worker可见的
    -- 都可以往该worker发送消息
    -- 所以即使一个进程有多个节点，只发起一个连接到master节点即可。
end

return Cluster
