-- 一些用来测试服务器的ai action

local Test = oo.class( ... )

-- 生成一些长短不一的字符串，用来检验服务器返回的包是否完整

local max_pkt = 65535 -- 测试socket拆包完整性才用65535,平时不用这么大
local random_str = { "",}

local function random_one( max_len )
    local chars = {}
    for len = 1,max_len do
        table.insert(chars,string.char(math.random(48,126)))
    end

    table.insert(random_str,table.concat(chars))
end

for idx = 1,200 do
    random_one( math.random(1,max_pkt) )
end

random_one( max_pkt )-- 保证最大临界值一定会出现

PRINT("random LARGE string finish !!!")

function Test:ping(ai)
    local entity = ai.entity

    ai.ping_ctx = random_str[math.random(1,#random_str)]
    ai.ping_ts = (ai.ping_ts or 0) + 1
    entity:send_pkt( CS.PLAYER_PING,
        {
            index = ai.ping_ts,
            context = ai.ping_ctx
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
    local ai = entity.ai
    ASSERT( pkt.context == ai.ping_ctx,string.len(ai.ping_ctx))
    PRINT("ping:",pkt.index)
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
