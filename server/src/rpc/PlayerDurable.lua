-- 针对单个玩家的可靠RPC调用
PlayerDurable = {}

--[[
1. 和durable一样，只解决网络问题、玩家下线问题，不解决宕机问题

2. 不要滥用这个模块，性能很低。一般的功能应该放它本身的离线模块，比如mail_off。而不是
全部依赖PlayerDurable。否则合服或者离线管理会非常麻烦。

3. PlayerDurable仅在game线程使用，因此路由地址应该是game线程地址
    理论上内部可以根据pid自动路由，但那样做的话和其他rpc调用接口不一致反而不好用
]]




-- 持久化数据（存库，正常关服后重启可恢复）
local this = memory("PlayerDurable", {
    recv = {},    -- [addr] = index 已接收的最大index
    send = {},    -- [index] = {addr, name, params}
    checked = {}, -- [pid] = true 已经检查过玩家在线状态的pid列表，避免重复检查
    epoch = time.game_time(),    -- 每次启动递增
    counter = 0,  -- 当前计数器
})

local BIT = 20
local MAX_COUNTER = 2 ^ BIT - 1

local send = Rpc.send
local parse_name_func = Rtti.parse_name_func

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
function PlayerDurable.on_recv(src, index, name, pid, ...)
    if "number" ~= type(pid) then
        error("PlayerDurable.on_recv invalid pid")
    end

    local s = PlayerOff.get_modify(pid)

    local durable = s.durable
    if not durable then
        durable = {
            recv = {},
            pending = {},
        }
        s.durable = durable
    end

    local old = durable.recv[src]
    if is_newer(index, old) then
        durable.recv[src] = index
        table.insert(durable.pending, {
            index = index,
            name = name,
            params = {...},
        })

        local func = parse_name_func(name)
        if func then
            func(...)
        else
            eprint("player durable on_recv no func", name)
        end
    else
        print("durable drop", name, index, old)
    end

    -- 无论是否重复都发ack（之前的ack可能丢失）
    Send[src].PlayerDurable.on_ack(index)

    -- 当前必定是玩家所在服务器的game线程，数据放到playeroff模块就算调用成功(从发起端的Durable移除数据)
    -- 至于什么时候真正调用，得看玩家什么时候在线
    local player = PlayerMgr.get_player(pid)
    if player then
    end
end

-- 收到发送确认，清理待确认记录
-- @param index string 唯一index
function PlayerDurable.on_ack(index)
    this.send[index] = nil
end

-- 发送一条durable消息
-- @param addr number 目标地址
-- @param name string 函数名
-- @param index string 唯一index
local function durable_do_send(addr, name, ...)
    local index = next_index()

    this.send[index] = {
        addr = addr,
        name = name,
        params = {...},
    }
    Send[addr].PlayerDurable.on_recv(
        LOCAL_ADDR, index, name, ...)
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
        return durable_do_send(addr, name, ...)
    end
end

local function on_login(player, is_new)
    if is_new then return end

    local s = PlayerOff.has_storage(player.pid)
    if not s or not s.durable then return end

    -- 收集需要重发的消息
    local pending = {}
    for index, info in pairs(s.durable) do
        pending[#pending + 1] = {
            index = index, info = info
        }
    end

    if #pending == 0 then return end

    -- 按index字符串排序即可保证有序到达
    table.sort(pending, function(a, b)
        return is_newer(b.index, a.index)
    end)

    pinfof("player durable resend %d msgs", player, #pending)
    for _, p in ipairs(pending) do
        local info = p.info
        Send[addr].PlayerDurable.on_recv(
            LOCAL_ADDR, p.index, info.name, table.unpack(info.params))
    end
end

Rpc.set_metatable(PlayerDurable, durable_func_factory)
script_loaded(function()
    Event.reg(EV.LOGIN, on_login)

    if LOCAL_ADDR ~= GAME_ADDR then
        PlayerDurable.on_recv = function()
            error("PlayerDurable should only be used in game thread")
        end
    end
end)
