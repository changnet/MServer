-- move.lua
-- xzc
-- 2018-01-20

-- 处理移动相关逻辑

local MT = require "modules.move.move_header"
local ET = require "modules.entity.entity_header"
local ABT = require "modules.attribute.attribute_header"

local Move = oo.class( ... )

function Move:__init(entity)
    self.entity = entity
end

-- 移动到对应坐标
-- @way:移动方式，普通、跳跃、飞行、传送...
function Move:move_to(way,pix_x,pix_y)
    -- 目标位置是否可行走
    local scene = self.entity:get_scene()
    if not scene or not scene:can_pass(pix_x,pix_y,true) then
        ERROR("move_to dest (%d,%d) can NOT pass",pix_x,pix_y)
        return
    end

    -- 行走、跳跃等需要花时间的另外处理
    -- 传送这种不需要花时间的是直接到目的地
    if MT.WALK == way or MT.JUMP == way or MT.FLY == way then
        return self:moving_to(scene,way,pix_x,pix_y)
    elseif MT.TELEPORT == way then
        assert(false) -- 待实现
    else
        ERROR("move_to unknow way:%d",way)
    end
end

local move_pkt = {}
-- 移动到对应位置
function Move:moving_to(scene,way,pix_x,pix_y)
    local ms_now = ev:ms_time()

    -- 先更新下当前位置，然后从现在开始计算移动
    self:moving( ms_now )
    self.ms_move = ms_now

    self.way = way
    self.dest_x = pix_x
    self.dest_y = pix_y

    move_pkt.handle = self.entity.eid
    move_pkt.way    = way
    move_pkt.pix_x  = pix_x
    move_pkt.pix_y  = pix_y

    -- 广播给玩家我开始移动
    -- 暂时不用发给自己，前端和后端位置不用一致，只要在允许范围内即可
    -- local to_me = ET.PLAYER == self.entity.et
    scene:broadcast_to_interest_me(self.entity,ENTITY.MOVE,move_pkt)
end

-- 停止移动
local pos_pkt = {}
function Move:raw_stop()
    self.way = nil
    self.ms_move = nil
    self.dest_x  = nil
    self.dest_y  = nil

    local entity = self.entity

    -- 广播给周围的玩家，停止移动
    pos_pkt.handle = entity.eid
    pos_pkt.pix_x  = entity.pix_x
    pos_pkt.pix_y  = entity.pix_y

    local to_me = ET.PLAYER == entity.et

    local scene = entity:get_scene()
    scene:broadcast_to_interest_me(entity,ENTITY.POS,pos_pkt,to_me)
end

-- 更新位置再停止移动
function Move:stop()
    -- 如果更新位置不成功，那么已经强制停止了，就不需要再次停止
    if not self:moving( ev:ms_time(),true ) then return end

    return self:raw_stop()
end

-- 定时处理玩家的位置
-- 广播实体的出现
local list_in = {}
local list_out = {}
function Move:moving( ms_now,no_time_chk )
    -- 移动方式不存在表示当前没在移动
    if not self.way or MT.NONE == self.way then return true end

    -- TODO:这个函数的调用间隔要精准控制。太频繁玩家每次移动还在同一个格子就浪费性能
    -- 太久才调用一次，则服务端无法及时广播移动给周围的人，也无法及时广播周围实体出现
    if not no_time_chk and ms_now - self.ms_move < 1000 then return end

    local entity = self.entity
    local speed = entity:get_attribute( ABT.MOVE )
    if speed <= 0 then
        ERROR("moving speed zero")
        return true
    end

    local ms = ms_now - self.ms_move
    local dis = ms * speed / 1000 -- 行走的像素距离，转化为秒

    local x = entity.pix_x
    local y = entity.pix_y

    local x_dis = self.dest_x - x
    local y_dis = self.dest_y - y
    local dest_dis = math.sqrt( x_dis * x_dis + y_dis * y_dis )

    local new_x = nil
    local new_y = nil
    if dis >= dest_dis then
        new_x = self.dest_x
        new_y = self.dest_y

        self:raw_stop() -- 到达目的地
    else
        -- 用 平等线等分线段 计算出新的x、y坐标
        new_x = math.floor(x + dis * x_dis / dest_dis)
        new_y = math.floor(y + dis * y_dis / dest_dis)
    end

    self.ms_move = ms_now
    local scene = entity:get_scene()

    -- 移动幅度太小，不用处理
    if scene.aoi:is_same_pos(x,y,new_x,new_y) then
        entity.pix_x = new_x
        entity.pix_y = new_y
        return
    end

    -- 新位置是否可行走
    if not scene:can_pass(new_x,new_y,true) then
        self:raw_stop()
        ERROR("moving (%f,%f) can NOT pass",new_x,new_y)
        return false
    end

    entity.pix_x = new_x
    entity.pix_y = new_y

    scene.aoi:update_entity(entity.eid,new_x,new_y,list_in,list_out)

    -- 这些玩家看不到我了，告诉他们我消失了
    scene:broadcast_entity_disappear(entity,list_out)
    -- 这些玩家看到我出现了
    scene:broadcast_entity_appear(entity,list_in)

    return true
end

return Move
