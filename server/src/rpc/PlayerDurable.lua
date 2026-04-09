-- 针对单个玩家的可靠RPC调用
-- 用法：
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

-- 接收durable消息（Send类型），由对端通过RPC调用
-- @param src number 发送方地址
-- @param name string 函数名
function PlayerDurable.on_recv(src, name, pid, ...)
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

    local index = next_index()
    table.insert(durable.pending, {
        index = index,
        name = name,
        params = {...},
    })

    -- 当前必定是玩家所在服务器的game线程，数据放到playeroff模块就算调用成功(从发起端的Durable移除数据)
    -- 至于什么时候真正调用，得看玩家什么时候在线
    local player = PlayerMgr.get_player(pid)
    if not player then return end

    Send[player.paddr].PlayerDurable.on_invoke(pid, index, name, ...)
end

-- 收到发送确认，清理待确认记录
-- @param index string 唯一index
function PlayerDurable.on_ack(pid, index)
    local s = PlayerOff.get_modify(pid)

    local durable = s.durable
    if not durable then
        eprint("PlayerDurable.on_ack no durable data", pid)
        return
    end

    local pending = durable.pending
    if not pending then
        eprint("PlayerDurable.on_ack no pending data", pid)
        return
    end

    for i, p in ipairs(pending) do
        if p.index == index then
            table.remove(pending, i)
            return
        end
    end
end

-- player线程收到调用
function PlayerDurable.on_invoke(pid, index, name, ...)
    local player = PlayerMgr.get_player(pid)
    -- 不在线直接丢掉，等下次登录再重发
    if not player then return end

    -- 数据已收到，回复确认，清理playeroff模块的待确认记录
    -- 如果下面的逻辑出错，也不能重发，那样会造成奖励重复发放
    Send[GAME_ADDR].PlayerDurable.on_ack(pid, index)

    local s = Player.get_data(player)

    local old = s.durable_idx
    if not is_newer(index, old) then
        -- 已经处理过了，可能是重复消息，直接丢掉
        print("PlayerDurable.on_invoke duplicate msg", player, index, old)
        return
    end

    s.durable_idx = index
    local func = parse_name_func(name)
    if not func then
        eprint("PlayerDurable.on_invoke invalid func", name)
        return
    end

    func(player, ...)
end

-- Durable factory函数
local function durable_func_factory(name, addr)
    return function(...)
        Durable[addr].PlayerDurable.on_recv(LOCAL_ADDR, name, ...)
    end
end

local function on_login(player, is_new)
    if is_new then return end

    local pid = player.pid
    local s = PlayerOff.has_storage(pid)
    if not s or not s.durable then return end

    local pending = s.durable.pending
    if not pending or #pending == 0 then return end

    pinfof("player durable resend %d msgs", player, #pending)

    local addr = player.paddr
    for _, p in ipairs(pending) do
        local info = p.info
        Send[addr].PlayerDurable.on_invoke(
            pid, p.index, info.name, table.unpack(info.params))
    end
end

Rpc.set_metatable(PlayerDurable, durable_func_factory)
script_loaded(function()
    Event.reg(EV.LOGIN, on_login, 65535)

    if LOCAL_ADDR ~= GAME_ADDR then
        PlayerDurable.on_recv = function()
            error("PlayerDurable addr should only be game thread")
        end
    end
end)
