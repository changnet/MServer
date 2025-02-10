-- 集群
local Cluster = {}

--[[
1. 在配置实现cluster配置
node = "gateway1", -- 当前节点（名字 + 索引）
cluster =
{
    -- 同一个类型的节点，只能有一个master(可以没有master)
    -- 一个节点的port = 0表示该节点只接收master中转的数据，不开启监听
    gateway1 = {host = "::1", port = 20001, master = 1},
    gateway2 = {host = "::1", port = 20002},
    db1 = {host = "::1", port = 20003},
    db2 = {host = "::1", port = 20004},
}

2. 进程启动时，如果cluster存在，根据cluster和node判断当前进程是否启用集群，是主还是从。主则开启监听，从则连接。
3. 一个worker类型可以配置一个master。如果配置了master，则往一个节点发送消息时，如果没有直达链接，则通过master中转
    master初始化时，会同步节点到所有worker
4. 数据全部由一个master转发，显然性能有限，并且延迟也会高些。因此假如该节点配置了host和port，其他节点可以直连
    直连需要手动连接，在配置中没有体现
5. 每个集群节点都会构建一个ClusterWorker，伪装成本地worker，所以rpc调用对集群节点没有差异
    如果没有master，节点数据不会统一同步。只能代码中手动连接才能获得连接的节点信息，才能发送消息

master通常用于调度（负载均衡）。比如场景节点开了10个，一个新玩家进入时，需要和master查询一个压力最小的节点。
后续数据可直接和分配好的节点通信。

集群通过tcp通信,消息默认都是不安全的，网络断开可能会导致消息丢失。玩家的消息一般是可丢弃的（比如场景中的移动请求）
但服务器之间的通信可能很重要，因此重要的数据需要手动使用Durable消息机制发送。

]]

return Cluster
