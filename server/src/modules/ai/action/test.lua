-- 一些用来测试服务器的ai action
Test = {}

function Test.ping(ai)
    if ai.ping_time then return end -- 上一次的还没返回

    local entity = ai.entity

    ai.ping_ts = (ai.ping_ts or 0) + 1
    ai.ping_time = ev:steady_clock()

    entity:send_pkt(PLAYER.PING, {})
end

function Test.gm(ai)
    -- gm测试
    local entity = ai.entity
    -- entity:send_pkt( MISC.WELCOME_GET,{} )
    entity:send_pkt(CHAT.DOCHAT, {channel = 1, context = "@ghf"})
    entity:send_pkt(CHAT.DOCHAT, {channel = 1, context = "@add_gold 9"})
    entity:send_pkt(CHAT.DOCHAT, {channel = 1, context = "@add_item 10000 1"})
    entity:send_pkt(CHAT.DOCHAT, {channel = 1, context = "@add_item 10001 1"})
    entity:send_pkt(CHAT.DOCHAT, {channel = 1, context = "@add_item 20001 1"})
end

function Test.chat(ai)
    ai.entity:send_pkt(CHAT.DOCHAT, {channel = 1, context = "12345678"})
end

-- ************************************************************************** --

function Test.on_ping(entity, ecode, pkt)
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

function Test.on_chat(entity, ecode, pkt)
    printf("chat: %d say %s", pkt.pid, pkt.context)
end

-- ************************************************************************** --


AndroidMgr.reg(PLAYER.PING, Test.on_ping)
AndroidMgr.reg(CHAT.DOCHAT, Test.on_chat)

return Test
