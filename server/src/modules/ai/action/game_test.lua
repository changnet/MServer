-- 一些零碎的测试服务器中游戏相关逻辑测试
GameTest = {}

local chat_cache = {}
local ping_pkt = {channel = 0xFFFF}
local ping_perf_info = {}

-- 随机一个字符串
local function randomStr(cache, max_cache, max_len)
    -- 如果已经创建足够多的缓存，就在旧的里随机
    if #cache >= max_cache then
        local index = math.random(#cache)
        return cache[index]
    end

    -- 包长最大64kb，这里随机60kb
    local len = math.random(1, max_len)

    local tbl = {}
    for _ = 1, len do
        table.insert(tbl, string.char(math.random(65, 90)))
    end

    local context = table.concat(tbl)
    table.insert(cache, context)

    return context
end

function GameTest.gm(ai)
    -- gm测试
    local entity = ai.entity
    -- 只运行一次的gm
    if not ai.once_gm then
        ai.once_gm = 1
        GameTest.chat(ai)
        entity:send_pkt(M.ChatMsg, {channel = 1, context = "@res 1 99999"})
        entity:send_pkt(M.ChatMsg, {channel = 1, context = "@res 2 99999"})

        entity:send_pkt(M.ChatMsg, {channel = 1, context = "@res 10000 1"})
        entity:send_pkt(M.ChatMsg, {channel = 1, context = "@res 10001 1"})
        entity:send_pkt(M.ChatMsg, {channel = 1, context = "@res 20001 1"})
        entity:send_pkt(M.ChatMsg, {channel = 1, context = "@mail test"})
        entity:send_pkt(M.ChatMsg, {channel = 1, context = "@test ikey"})
        entity:send_pkt(M.ChatMsg, {channel = 1, context = "@test deadloop"})

        -- 避免刚刚发送的大批指令在服务器排队阻塞，导致首次 ping1 包的往返耗时严重偏高
        -- 这里延迟N秒后再开始测延迟
        ai.ping_latency_time = os.time() + 3
    elseif ai.ping_latency_time and os.time() >= ai.ping_latency_time then
        ai.ping_latency_time = nil

        print("ping latency test, remove msg debug ...")
        entity.msg_dbg = nil -- 打印日志会严重影响测试结果
        GameTest.ping_latency(ai)
    end
end

local function send_ping(entity, ping)
    local ts = ping.ts + 1
    ping_pkt.context = string.format("@test %s %d", ping.type, ts)
    entity:send_pkt(M.ChatMsg, ping_pkt)

    ping.ts = ts
    ping.beg = Engine.steady_clock()
end

local function reset_ping_context(ping, ptype)
    ping.type = ptype or "ping1"
    ping.ts = 0
    ping.time = 0
    ping.max_time = 0
    ping.min_time = 0xFFFFFFFF
end

local function ping_perf_cb(entity)
    local ping = entity.ai.ping
    if not ping then return end

    local cnt = ping.ts or 0
    local tot = ping.time or 0
    local mn = ping.min_time or 0
    local mx = ping.max_time or 0

    local s = ping_perf_info

    assert(s.type == ping.type)

    s.ts = s.ts + cnt
    s.time = s.time + tot
    if mn < s.min_time then s.min_time = mn end
    if mx > s.max_time then s.max_time = mx end

    ping_perf_info.completed = ping_perf_info.completed + 1
    if ping_perf_info.completed < ping_perf_info.total then return end

    printf("ALL %s avg: %.3f, min: %d, max: %d, bots: %d",
        s.type, s.time/s.ts, s.min_time, s.max_time, s.completed)

    if s.type ~= "ping1" then return end

    s.completed = 0
    reset_ping_context(ping_perf_info, "ping2")

    local bots = BotMgr.get_bots()
    for _, bot in pairs(bots) do
        bot.ai.ping = nil
        GameTest.ping_latency(bot.ai, ping_perf_cb, "ping2")
    end
end

-- 测试服务器性能，对所有bots发起GameTest.ping_latency
function GameTest.ping_perf(now)
    if ping_perf_info.run then return end

    if now < (ping_perf_info.next or 0)  then return end

    local bots = BotMgr.get_bots()
    -- 检测所有机器人是否登录完成
    for _, bot in pairs(bots) do
        if bot.ai.state ~= AST.ON then
            ping_perf_info.next = now + 1000
            print("bot not login, wait ...")
            return
        end
    end

    -- 开始进行测试
    ping_perf_info.run = 1

    -- 初始化统计信息
    ping_perf_info.type = "ping1"
    ping_perf_info.total = #bots
    ping_perf_info.completed = 0
    reset_ping_context(ping_perf_info)

    for _, bot in pairs(bots) do
        GameTest.ping_latency(bot.ai, ping_perf_cb)
    end
end

-- 测试服务器延迟
function GameTest.ping_latency(ai, cb, ptype)
    local ping = ai.ping
    if ping then
        print("GameTest.ping_latency already beg")
        return
    end -- 上一次的还没返回

    local entity = ai.entity

    ping = {
        max = 100,
        beg = 0,
    }
    reset_ping_context(ping, ptype)

    --[[
    bot_4294968211 ping1 100 times finish, avg: 0.020, min: 0, max: 1
    bot_4294968211 ping2 100 times finish, avg: 0.110, min: 0, max: 1
    ]]
    ping.cb = cb or function()
        printf("%s %s %d times finish, avg: %.3f, min: %d, max: %d",
            entity.name, ping.type, ping.max,
            ping.time / ping.ts, ping.min_time, ping.max_time
        )

        if "ping1" == ping.type then
            reset_ping_context(ping, "ping2")

            send_ping(entity, ping)
        end
    end
    ai.ping = ping

    send_ping(entity, ping)
end

local function s_ping(entity, ecode, pkt)
    local ai = entity.ai
    local ping = ai.ping

    local ms = Engine.steady_clock() - ping.beg

    ping.time = ping.time + ms
    if ms > ping.max_time then ping.max_time = ms end
    if ms < ping.min_time then ping.min_time = ms end

    if tonumber(pkt.context) ~= ping.ts then
        print("ping ts error", entity.name, pkt.context, ping.ts)
        return
    end

    if ping.ts >= ping.max then
        ping.cb(entity)
    else
        send_ping(entity, ping)
    end
end

-- 聊天测试，通常用于测试包的完整性
-- 放到ping那里测试了，聊天是广播，人一多就会造成缓冲区溢出
function GameTest.chat(ai)
    if ai.last_chat then
        ai.fail_chat = 1 + (ai.fail_chat or 0)
        if 0 == ai.fail_chat % 10 then
            print("ai fail chat", ai.entity.name, ai.fail_chat)
        end
    end

    local str = randomStr(chat_cache, 96, 24)

    local pkt = {channel = 1, context = str}
    ai.last_chat = pkt
    ai.entity:send_pkt(M.ChatMsg, pkt)
end

local function s_chat_msg(entity, pkt)
    -- printf("chat: %d say %s", pkt.pid, pkt.context)

    if pkt.channel == 0xFFFF then
        return s_ping(entity, 0, pkt)
    end

    if pkt.pid == entity.pid then
        entity.ai.fail_chat = nil

        local context = nil
        if entity.ai.last_chat then context = entity.ai.last_chat.context end

        if pkt.context ~= context then
            print("ai chat error", pkt.context, context)
        end

        entity.ai.last_chat = nil
    end
end

-- ************************************************************************** --

BotMgr.reg(M.ChatMsg, s_chat_msg)

return GameTest
