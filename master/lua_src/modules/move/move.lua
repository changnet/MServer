-- move.lua
-- xzc
-- 2018-01-20

-- 处理移动相关逻辑

local MT = require "modules.move.move_header"
local ET = require "modules.entity.entity_header"
local ABT = require "modules.attribute.attribute_header"

local Move = oo.class( nil,... )

function Move:__init(entity)
    self.entity = entity
end

-- 移动到对应坐标
-- @way:移动方式，普通、跳跃、飞行、传送...
function Move:move_to(way,pix_x,pix_y)
    -- 目标位置是否可行走
    local scene = self.entity:get_scene()
    if not scene or scene:can_pass(pix_x,pix_y) then
        ELOG("move_to dest (%d,%d) can NOT pass",pix_x,pix_y)
        return
    end

    -- 行走、跳跃等需要花时间的另外处理
    -- 传送这种不需要花时间的是直接到目的地
    if MT.WALK == way or MT.JUMP == way or MT.FLY == way then
        return self:moving_to(scene,way,pix_x,pix_y)
    elseif MT.TELEPORT == way then
        assert(false) -- 待实现
    else
        ELOG("move_to unknow way:%d",way)
    end
end

local move_pkt = {}
local watch_list = {}
-- 移动到对应位置
function Move:moving_to(scene,way,pix_x,pix_y)
    local ms_now = ms_time

    -- 先更新下当前位置，然后从现在开始计算移动
    self:moving( ms_now )
    self.ms_move = ms_now

    self.way = way
    self.dest_x = pix_x
    self.dest_y = pix_y

    -- 广播给玩家我开始移动
    scene.aoi:get_watch_me_entitys(self.entity.eid,watch_list,ET.PLAYER)

    local pid_list = {}

    -- 如果自己是玩家，那么移动消息也发给自己
    if ET.PLAYER == self.entity.et then
        table.insert(pid_list,self.entity.pid)
    end

    for idx = 1,tmp_list.n do
        local tmp_entity = g_entity_mgr:get_entity( tmp_list[idx] )

        -- 如果在我周围的是一个玩家，告诉他我出现了
        if ET.PLAYER == tmp_entity.et then
            table.insert(pid_list,tmp_entity.pid)
        end
    end

    if not table.empty(pid_list) then
        -- 1表示底层按玩家pid广播
        move_pkt.handle = self.entity.eid
        move_pkt.way    = way
        move_pkt.pix_x  = pix_x
        move_pkt.pix_y  = pix_y
        g_network_mgr:clt_multicast( 1,pid_list,SC.ENTITY_MOVE,move_pkt )
    end
end

-- 定时处理玩家的位置
-- 广播实体的出现
function Move:moving( ms_now )
    -- 移动方式不存在表示当前没在移动
    if not self.way or MT.NONE == self.way then return end

    -- TODO:这个函数的调用间隔要精准控制。太频繁玩家每次移动还在同一个格子就浪费性能
    -- 太久才调用一次，则服务端无法及时广播移动给周围的人，也无法及时广播周围实体出现

    local speed = self.entity:get_attribute( ABT.MOVE )
    if speed <= 0 then
        ELOG("moving speed zero")
        return
    end

    local ms = ms_now - self.ms_move
    local dis = ms * speed / 1000 -- 行走的像素距离，转化为秒

    local x = self.entity.pix_x
    local y = self.entity.pix_y

    local x_dis = self.dest_x - x
    local y_dis = self.dest_y - y
    local dest_dis = math.sqrt( x_dis * x_dis + y_dis * y_dis )

    local new_x = nil
    local new_y = nil
    if dis >= dest_dis then
        new_x = self.dest_x
        new_y = self.dest_y
    else
        -- 用 平等线等分线段 计算出新的x、y坐标
        new_x = dis * x_dis / dest_dis
        new_y = dis * y_dis / dest_dis
    end

    -- 新位置是否可行走
    local scene = self.entity:get_scene()
    if not scene or scene:can_pass(new_x,new_y) then
        ELOG("moving (%d,%d) can NOT pass",new_x,new_y)
        return
    end
end

return Move
