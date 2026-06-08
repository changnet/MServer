-- 一些零碎的测试服务器中游戏相关逻辑测试
GameTest = {}

local chat_cache = {}
local ping_cache = {}

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
    end
end

-- 测试服务器延迟
function GameTest.ping(ai)
    if ai.ping_time then return end -- 上一次的还没返回

    local entity = ai.entity

    local str = randomStr(ping_cache, 1024, 60 * 1024)

    ai.ping_ts = (ai.ping_ts or 0) + 1
    ai.ping_time = Engine.steady_clock()
    ai.ping_verify = str

    entity:send_pkt(M.PlayerPing, {verify = str})
end

local function s_ping(entity, ecode, pkt)
    local ai = entity.ai

    local beg = ai.ping_time

    ai.ping_time = nil
    if ai.ping_verify ~= pkt.verify then
        print("ping verify error", entity.name, ai.ping_verify, pkt.verify)
    end

    -- 服务器不忙的情况下，延迟是1~5毫秒左右.60帧则是16ms以下
    local ms = Engine.steady_clock() - beg
    print("ping", entity.name, ai.ping_ts, ms, #pkt.verify)
    for _, delay in pairs(pkt.delay) do
        if (delay.time or 0) + ms > 10 then
            print("     latency too large", delay.name, (delay.time or 0) + ms)
        end
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

BotMgr.reg(M.PlayerPing, s_ping)
BotMgr.reg(M.ChatMsg, s_chat_msg)

return GameTest
