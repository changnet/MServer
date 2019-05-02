-- 一些用来测试服务器的ai action

local Test = oo.class( nil,... )

function Test:ping(ai)
    local entity = ai.entity

    ai.ping_ts = (ai.ping_ts or 0) + 1
    entity:send_pkt( CS.PLAYER_PING,
        {
            x = entity.pid,
            y = ai.ping_ts,
            z = (ai.ping_idx or 1) + 1,
            way = ev:time(),
            target = { 1,2,3,4,5,6,7,8,9 },
            say = "android ping test android ping test android ping test android ping test android ping test"
        } )
end

function Test:gm(ai)
    -- gm测试
    local entity = ai.entity
    -- entity:send_pkt( CS.MISC_WELCOME_GET,{} )
    entity:send_pkt( CS.CHAT_DOCHAT,{ channel = 1,context = "@ghf" } )
    entity:send_pkt( CS.CHAT_DOCHAT,{ channel = 1,context = "@add_gold 9" } )
    entity:send_pkt( CS.CHAT_DOCHAT,{ channel = 1,context = "@add_item 10000 1" } )
    entity:send_pkt( CS.CHAT_DOCHAT,{ channel = 1,context = "@add_item 10001 1" } )
    entity:send_pkt( CS.CHAT_DOCHAT,{ channel = 1,context = "@add_item 20001 1" } )
end

function Test:chat(ai)
    ai.entity:send_pkt( CS.CHAT_DOCHAT,{ channel = 1,context = "12345678" } )
end

-- ************************************************************************** --

function Test:on_ping( entity,ecode,pkt )
    PRINT("ping:",pkt.y)
    -- vd(pkt)
end

function Test:on_chat( entity,ecode,pkt )
    PRINTF( "chat: %d say %s",pkt.pid,pkt.context )
end

-- ************************************************************************** --

local function cmd_cb( cmd,cb )
    local raw_cb = function (android,errno,pkt)
        return cb(Test,android,errno,pkt)
    end

    g_android_cmd:cmd_register( cmd,raw_cb )
end

cmd_cb( SC.PLAYER_PING,Test.on_ping)
cmd_cb( SC.CHAT_DOCHAT,Test.on_chat)

return Test
