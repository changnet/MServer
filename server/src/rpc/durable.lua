-- Send的持久化rpc调用
Durable = {}

-- Callback的持久化调用（回调函数必须是Rtti中的函数）
Duraback = {}

--[[
1. Durable给每个调用分配一个id，如果未收到确认则在重连后重发，保证对端一定能收到

2. 它只保证因为对端网络中断、关服时能重发，不保证进程崩溃等异常情况。保证崩溃需要类似
数据库那样的日志机制，代价太高，还不如用第三方消息队列RocketMQ等

3. 从第2点上可以看出，同一进程的worker用这个是没有意义的。它用于跨进程socket通信，例如：
在跨服上往游戏服发一封邮件，用Durable不用担心恰好关服了邮件丢失

4. 无法使用Call同步调用机制，只能使用Send异步机制。因为这个调用可能重启后才调用成功，已
经不存在原来的调用上下文。即使异步调用的返回，也没有了上下文。

5. 消息重发时，是按原addr发送。这时候玩家可能已经下线或者更换了所在的地址。因此Durable
调用一般不能针对玩家对象操作

]]

local this = memory("Durable", {
    recv = {}, -- [addr] = max_index,
    send = {}, -- [index] = {addr = 1,func = xxx, params = {}} -- 已发送未返回的调用
})

-- 新的worker重连上来，如果有数据要重发
local function on_worker_ready(addr)
end

Event.reg(EV.WORKER_BOTH_READY, on_worker_ready, 0xFFFF)
