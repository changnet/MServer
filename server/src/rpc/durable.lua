-- 持久化RPC调用
-- 保证跨进程消息在网络中断、关服时能重发，不保证进程崩溃

Durable = {}  -- Send的持久化调用

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

6. 原本这里还想实现一个Callback机制的，但发现这样不仅发送需要持久，返回也需要持久。所以
如果需要回调，先用Durable发一条，对端再用Durable返回一条即可
]]

local send = Rpc.send
local parse_name_func = Rtti.parse_name_func

-- 持久化数据（存库，正常关服后重启可恢复）
local this = storage("Durable", {
    recv = {},    -- [addr] = index 已接收的最大index
    send = {},    -- [index] = {addr, name, params}
    epoch = 0,    -- 每次启动递增
    counter = 0,  -- 当前计数器
})

local BIT = 20
local MAX_COUNTER = 2 ^ BIT - 1

-- 生成唯一index
-- @return string "epoch_counter"
local function next_index()
    local counter = this.counter + 1
    if counter > MAX_COUNTER then
        this.epoch = this.epoch + 1
        counter = 1
    end

    this.counter = counter

    return (this.epoch << BIT | this.counter)
end

-- 判断新index是否比已记录的更新
-- @param new_index string
-- @param old_index string|nil
-- @return boolean
local function is_newer(new_index, old_index)
    if not old_index then return true end

    return new_index > old_index
end

-- 判断目标地址是否需要持久化（仅跨进程需要）
-- @param addr number 目标地址
-- @return boolean
local function need_durable(addr)
    local data = WorkerData[addr]
    if not data then return true end
    return data.mode ~= Worker.THREAD
end

-- 接收durable消息（Send类型），由对端通过RPC调用
-- @param src number 发送方地址
-- @param index string 唯一index（用于ack和去重）
-- @param name string 函数名
function Durable.on_recv(src, index, name, ...)
    local recv = this.recv
    local old = recv[src]

    -- 确认有收到数据就回包
    -- 如果下面的逻辑出错也不能重发，避免重复调用造成奖励被刷
    Send[src].Durable.on_ack(index)

    if is_newer(index, old) then
        recv[src] = index

        local func = parse_name_func(name)
        if func then
            func(...)
        else
            eprint("durable on_recv no func", name)
        end
    else
        print("durable drop", name, index, old)
    end


end

-- 收到发送确认，清理待确认记录
-- @param index string 唯一index
function Durable.on_ack(index)
    this.send[index] = nil
end

-- Durable factory函数
local function durable_func_factory(name, addr)
    if not need_durable(addr) then
        -- 同进程退化为普通Send
        return function(...)
            return send(name, addr, ...)
        end
    end

    return function(...)
        local index = next_index()

        this.send[index] = {
            addr = addr,
            name = name,
            params = {...},
        }
        Send[addr].Durable.on_recv(
            LOCAL_ADDR, index, name, ...)
    end
end

-- worker重连时，按顺序重发未确认的消息
-- @param addr number 重连的worker地址
-- @param mode number worker模式
local function on_worker_ready(addr, mode)
    if mode == Worker.THREAD then return end

    -- 收集需要重发的消息
    local pending = {}
    for index, info in pairs(this.send) do
        if info.addr == addr then
            pending[#pending + 1] = {
                index = index, info = info
            }
        end
    end

    if #pending == 0 then return end

    -- 按index字符串排序即可保证有序到达
    table.sort(pending, function(a, b)
        return is_newer(b.index, a.index)
    end)

    printf("durable resend %d msgs to addr %d",
        #pending, addr)
    for _, p in ipairs(pending) do
        local info = p.info
        Send[addr].Durable.on_recv(
            LOCAL_ADDR, p.index, info.name, table.unpack(info.params))
    end
end

-- 系统启动完成，storage已加载，初始化durable
local function on_ready()
    -- 递增epoch，重启后index一定比之前的大
    this.epoch = time.game_time()
    this.counter = 0

    printf("durable ready, epoch = %d, pending = %d",
        this.epoch, table.size(this.send))

    for addr, data in pairs(WorkerData) do
        if data.status == Worker.READY then
            on_worker_ready(addr, data.mode)
        end
    end
end

Rpc.set_metatable(Durable, durable_func_factory)
if this then -- 部分worker没有storage功能，不需要durable模块
    script_loaded(function()
        Event.reg(EV.READY, on_ready)
        Event.reg(EV.WORKER_BOTH_READY, on_worker_ready, 0xFFFF)
    end)
end
