-- worker路由
WorkerRoute = {}

--[[
此模块负责两个功能

1. 记录玩家和各个功能分配到哪个worker，比如场景有多个worker，玩家进入某一个worker后
要记录玩家所在worker才能把移动、攻击等协议转发过去

2. 负责worker的分配，比如database有10个，存背包时到底用哪一个

分配的方式有两种：
1. 按压力动态分配，然后记录分配到哪个worker
2. 按规则分配，每次用规则计算出对应的worker
]]

-- 获取玩家所在worker地址
function WorkerRoute.get_player_addr(pid, wtype)
    assert(false)
end
