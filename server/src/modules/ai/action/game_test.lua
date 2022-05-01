-- 一些零碎的测试服务器中游戏相关逻辑测试
GameTest = {}

local chatCache = {}

-- 测试服务器延迟
function GameTest.ping(ai)
    if ai.ping_time then return end -- 上一次的还没返回

    local entity = ai.entity

    ai.ping_ts = (ai.ping_ts or 0) + 1
    ai.ping_time = ev:steady_clock()

    entity:send_pkt(PLAYER.PING, {})
end

function GameTest.gm(ai)
    -- gm测试
    local entity = ai.entity
    -- entity:send_pkt( MISC.WELCOME_GET,{} )
    entity:send_pkt(CHAT.DOCHAT, {channel = 1, context = "@ghf"})
    entity:send_pkt(CHAT.DOCHAT, {channel = 1, context = "@add_gold 9"})
    entity:send_pkt(CHAT.DOCHAT, {channel = 1, context = "@add_item 10000 1"})
    entity:send_pkt(CHAT.DOCHAT, {channel = 1, context = "@add_item 10001 1"})
    entity:send_pkt(CHAT.DOCHAT, {channel = 1, context = "@add_item 20001 1"})
end

-- 随机一个聊天字符串
local function randomChat()
    -- 如果已经创建足够多的缓存，就在旧的里随机
    local MAX_CACHE = 10240

    if #chatCache >= MAX_CACHE then
        local index = math.random(#chatCache)
        return chatCache[index]
    end

    -- 包长最大64kb，这里随机60kb
    local max_len = math.random(1, 60 * 1024)

    local tbl = {}
    for _ = 1, max_len do
        table.insert(tbl, string.char(math.random(65, 90)))
    end

    local context = table.concat(tbl)
    table.insert(chatCache, context)

    return context
end

-- 聊天测试，通常用于测试包的完整性
function GameTest.chat(ai)
    if ai.last_chat then
        ai.fail_chat = 1 + (ai.fail_chat or 0)
        if 0 == ai.fail_chat % 10 then
            print("ai fail chat", ai.entity.name, ai.fail_chat)
        end
    end

    local pkt = {channel = 1, context = randomChat()}
    ai.last_chat = pkt
    ai.entity:send_pkt(CHAT.DOCHAT, pkt)
end

-- ************************************************************************** --

function GameTest.on_ping(entity, ecode, pkt)
    local ai = entity.ai

    local beg = ai.ping_time

    ai.ping_time = nil

    -- 服务器不忙的情况下，延迟是1~5毫秒左右.60帧则是16ms以下
    local ms = ev:steady_clock() - beg
    print("ping", ai.ping_ts, ms)
    for _, delay in pairs(pkt.delay) do
        if (delay.time or 0) + ms > 10 then
            print("     latency too large", delay.name, (delay.time or 0) + ms)
        end
    end
end

function GameTest.on_chat(entity, ecode, pkt)
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


AndroidMgr.reg(PLAYER.PING, GameTest.on_ping)
AndroidMgr.reg(CHAT.DOCHAT, GameTest.on_chat)

return GameTest
