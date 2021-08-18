-- AI移动相关action
local MT = require "modules.move.move_header"

local Move = {}

local DG_MIN_SEC = 30
local DG_MAX_SEC = 180

-- 随机移动
-- 这个是机器人用来模拟客户端移动的。怪物AI是直接在服务器移动，不能用这个
function Move.random_move(ai)
    if ai.moving then return false end

    -- 在dungeon_mgr中初始化的场景大小
    local px = math.random(0, 128 * 64 - 1)
    local py = math.random(0, 64 * 64 - 1)
    ai.entity:send_pkt(ENTITY.MOVE, {way = MT.WALK, pix_x = px, pix_y = py})

    ai.moving = true
    ai.dx = px
    ai.dy = py
    printf("move to %d,%d(%d,%d)", math.floor(px / 64), math.floor(py / 64), px,
           py)
    return true
end

-- 玩家进入场景
function Move.on_enter_scene(entity, errno, pkt)
    local dungeon_id = pkt.dungeon_id or 0 -- pbc里int为0发不过来
    printf("%s(%d) enter scene %d:%d", entity.name, entity.pid, dungeon_id,
           pkt.scene_id)

    -- 设置位置信息
    entity.dungeon_id = dungeon_id
    entity.dungeon_hdl = pkt.dungeon_hdl
    entity.scene_id = pkt.scene_id
    entity.scene_hdl = pkt.scene_hdl
    entity.pix_x = pkt.pix_x
    entity.pix_y = pkt.pix_y

    entity.ai.moving = false
end

-- 某个实体移动
function Move.on_move(entity, errno, pkt)
    if entity.handle == pkt.handle then
        entity.ai.moving = true

        entity.pix_x = pkt.pix_x
        entity.pix_y = pkt.pix_y

        print("move my pos to", entity.name, pkt.pix_x, pkt.pix_y)
    else
        print("other move pos at", pkt.pix_x, pkt.pix_y)
    end
end

function Move.on_appear(entity, errno, pkt)
    print("entity appear:", pkt.name, pkt.pix_x, pkt.pix_y)
end

-- 实体消失
function Move.on_disappear(entity, errno, pkt)
    print("entity disappear:", pkt.handle)
end

-- 服务器强制重置实体位置
function Move.on_update_pos(entity, errno, pkt)
    if entity.handle == pkt.handle then
        entity.ai.moving = false

        entity.pix_x = pkt.pix_x
        entity.pix_y = pkt.pix_y
        print("update my pos at", entity.name, pkt.pix_x, pkt.pix_y)
    else
        print("update other pos at", pkt.pix_x, pkt.pix_y, pkt.handle)
    end
end

-- 切换副本，测试玩家在进程中切换
function Move.switch_dungeon(ai)
    local now = ev:time()
    if not ai.dungeon_time then
        ai.dungeon_time = now + math.random(DG_MIN_SEC, DG_MAX_SEC)
    end

    if ai.dungeon_time > now then return end

    -- 随机切换一个副本
    local id = math.random(1, 10)
    if id == (ai.fb_id or 0) then return end

    ai.entity:send_pkt(PLAYER.ENTERDUNGEON, {id = id})
    ai.dungeon_time = now + math.random(DG_MIN_SEC, DG_MAX_SEC)

    ai.fb_id = id
    local entity = ai.entity
    print("switch to new dungeon", entity.name, id)
end

AndroidMgr.reg(ENTITY.MOVE, Move.on_move)
AndroidMgr.reg(ENTITY.APPEAR, Move.on_appear)
AndroidMgr.reg(ENTITY.POS, Move.on_update_pos)
AndroidMgr.reg(ENTITY.DISAPPEAR, Move.on_disappear)
AndroidMgr.reg(ENTITY.ENTERSCENE, Move.on_enter_scene)

return Move
