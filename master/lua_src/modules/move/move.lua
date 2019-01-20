-- move.lua
-- xzc
-- 2018-01-20

-- 处理移动相关逻辑

local MT = require "modules.move.move_header"

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
        return self:moving_to(way,pix_x,pix_y)
    elseif MT.TELEPORT == way then
        assert(false) -- 待实现
    else
        ELOG("move_to unknow way:%d",way)
    end
end

-- 移动到对应位置
function Move:moving_to(way,pix_x,pix_y)
    self.way = way
    self.dest_x = pix_x
    self.dest_y = pix_y

    -- 广播给玩家我开始移动
end

-- 定时处理玩家的位置
-- 广播实体的出现
function Move:moving()
    -- 移动方式不存在表示当前没在移动
    if not self.way or MT.NONE == self.way then return end
end

return Move
