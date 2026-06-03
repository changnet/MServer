-- 专门用于处理玩家消息的队列
PlayerQueue = {}

--[[
1. 每个玩家一个线程有一个消息队列，避免消息中使用协程调用时，后一个消息在前一个未执行完就执行
    注意不同线程多个消息仍是并发执行的，一些状态在协程恢复后仍需要double check

2. 消息只在执行时持有协程，避免玩家大量发包触发协程大量创建

3. 对于异步、定时器调用，目前暂时没有处理。回调时由开发都自己二次检测玩家是否在线
    因为异步调用可能会在多个线程中挂起执行，甚至形成环形、死锁，没有好的办法处理这个问题

4. 有些线程消息回调没有玩家对象只有玩家id，这些线程不分配消息队列
    因为没有玩家对象的说明都是可以离线操作，不涉及玩家下线问题，自己处理好并发

5. 像帮派多人改名，商店抢购等，应该把多人的消息丢到一个queue里，而不是依赖单个玩家的队列

玩家下线策略
    1. 玩家下线时，要先检测是否有执行中的协程，防止把执行到一半的逻辑中断
    2. 如果队列中没有执行中的消息，直接下线退出
    3. 如果存在执行中的消息，等待执行中的消息完成或者超时下线

消息超时机制
    1. 消息进入队列时，发现上一个消息超过X秒，则打印日志及对应co的堆栈
    2. 消息即使超时，不丢弃，玩家可以继续等响应或重登录。但玩家下线的话将会被丢弃

这个模块作为底层模块，为了效率不使用oo.class
]]

local FuncQueue = require("queue.func_queue")

local queue_len = FuncQueue.len
local queue_push = FuncQueue.push

function PlayerQueue.push(player, func, ...)
    return queue_push(player.queue, func, ...)
end

local function wait_for_empty(player)
    local queue = player.queue
    if queue_len(queue) == 0 then return end

    queue:close()
    print("wait for empty queue", player.pid)
    for _ = 1, 10 do
        Timer.wait(500)
        if queue_len(queue) == 0 then return end
    end
    eprint("wait for empty queue timeout", player.pid)
end

function PlayerQueue.on_ensure_empty(pid)
    local player = PlayerMgr.get_player(pid)
    if not player then return end

    wait_for_empty(player)
end

-- 向各个线程确认玩家消息队列为空才可以安全下线
function PlayerQueue.ensure_empty(player)
    wait_for_empty(player)

    local pid = player.pid
    local sync_wtypes = Worker.get_key_type_list("pobj")

    for wtype in pairs(sync_wtypes) do
        local addr = Router.find_player_addr(pid, wtype)
        Call[addr].PlayerQueue.on_ensure_empty(pid)
    end
end

return PlayerQueue
