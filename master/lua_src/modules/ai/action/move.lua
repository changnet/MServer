-- AI移动相关action

local MT = require "modules.move.move_header"

local Move = oo.class( nil,... )

-- 随机移动
-- 这个是机器人用来模拟客户端移动的。怪物AI是直接在服务器移动，不能用这个
function Move:random_move(ai)
    if ai.moving then return false end

    -- 在dungeon_mgr中初始化的场景大小
    local px = math.random(0,128*64)
    local py = math.random(0,64*64)
    ai.entity:send_pkt( CS.ENTITY_MOVE,{ way = MT.WALK,pix_x = px,pix_y = py } )

    ai.moving = true
    return true
end

function Move:on_move(entity,errno,pkt)
end

-- ************************************************************************** --

local function cmd_cb( cmd,cb )
    local raw_cb = function (android,errno,pkt)
        return cb(Move,android,errno,pkt)
    end

    g_android_cmd:cmd_register( cmd,raw_cb )
end

cmd_cb( SC.ENTITY_MOVE,Move.on_move)

return Move
