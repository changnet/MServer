-- 玩家和worker路由管理
Router = {}

local this = memory("Router", {
    wpolicy = {}, -- worker类型对应的负载均衡策略
    pid_map = {}, -- [pid][wtype] = addr 玩家在对应worker类型的地址的映射
})

-- 默认的负载均衡策略（通过轮询依次分配到各个worker，适用于无状态的worker）
local function default_balance(wtype)
end

-- 根据worker类型查看玩家所在地址
function Router.find_player_addr(pid, wtype)
end

-- 更新玩家所在worker地址
function Router.update_worker_addr(pid, addr)
end

-- 根据worker类型，查找一个可用worker地址
function Router.find_worker_addr(wtype)
end

-- 设置指定类型worker的负载均衡策略
function Router.set_balance_policy(wtype, func)
    this.wpolicy[wtype] = func
end
