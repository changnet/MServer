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

-- 玩家进入场景
function Move:on_enter_scene(entity,errno,pkt)
    local dungeon_id = pkt.dungeon_id or 0 -- pbc里int为0发不过来
    PRINTF("%s(%d) enter scene %d:%d",
        entity.name,entity.pid,dungeon_id,pkt.scene_id)

    -- 设置位置信息
    entity.dungeon_id = dungeon_id
    entity.dungeon_hdl = pkt.dungeon_hdl
    entity.scene_id = pkt.scene_id
    entity.scene_hdl = pkt.scene_hdl
    entity.pix_x = pkt.pix_x
    entity.pix_y = pkt.pix_y

    entity.ai.moving = false
end

function Move:on_move(entity,errno,pkt)
    if entity.handle == pkt.handle then
        entity.ai.moving = true

        entity.pix_x = pkt.pix_x
        entity.pix_y = pkt.pix_y
        PRINT("move my pos to",entity.name,pkt.pix_x,pkt.pix_y)
    else
        PRINT("move other pos at",pkt.pix_x,pkt.pix_y)
    end
end

function Move:on_appear(entity,errno,pkt)
    PRINT("entity appear:",pkt.name,pkt.pix_x,pkt.pix_y)
end

-- 实体消失
function Move:on_disappear(entity,errno,pkt)
    PRINT("entity disappear:",pkt.handle)
end

-- 服务器强制重置实体位置
function Move:on_reset_pos(entity,errno,pkt)
    if entity.handle == pkt.handle then
        entity.ai.moving = false

        entity.pix_x = pkt.pix_x
        entity.pix_y = pkt.pix_y
        PRINT("reset my pos at",entity.name,pkt.pix_x,pkt.pix_y)
    else
        PRINT("reset other pos at",pkt.pix_x,pkt.pix_y)
    end
end

-- ************************************************************************** --

local function cmd_cb( cmd,cb )
    local raw_cb = function (android,errno,pkt)
        return cb(Move,android,errno,pkt)
    end

    g_android_cmd:cmd_register( cmd,raw_cb )
end

cmd_cb( SC.ENTITY_MOVE,Move.on_move)
cmd_cb( SC.ENTITY_APPEAR,Move.on_appear)
cmd_cb( SC.ENTITY_POS,Move.on_reset_pos)
cmd_cb( SC.ENTITY_DISAPPEAR,Move.on_disappear)
cmd_cb( SC.ENTITY_ENTERSCENE,Move.on_enter_scene)

return Move
