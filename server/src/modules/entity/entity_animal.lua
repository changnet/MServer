-- entity_animal.lua
-- xzc
-- 2019-01-20
-- 能够移动、攻击的实体，用于处理移动、攻击逻辑
local Move = require "modules.move.move"
local Entity = require "modules.entity.entity"
local AttributeSys = require "modules.attribute.attribute_sys"

local EntityAnimal = oo.class(..., Entity)

function EntityAnimal:__init(...)
    Entity.__init(self, ...)

    self.move = Move(self)
    self.abt_sys = AttributeSys() -- 战斗属性(血量、攻击力...)
end

-- 获取战斗属性
function EntityAnimal:get_attribute(abt)
    return self.abt_sys:get_one_final_attr(abt) or 0
end

-- 处理客户端请求移动
function EntityAnimal:do_move(pkt)
    -- 后端不处理寻路，前端发给后端的路线都是直线的，如果遇到转弯这些则必须分开发送
    -- 后端不严格检测线路是否可行，只会定时检测，遇到不可行走则会拉玩家到对应的位置

    -- TODO:pbc不发0过来，而是nil
    self.move:move_to(pkt.way, pkt.pix_x or 0, pkt.pix_y or 0)
end

-- 事件循环
function EntityAnimal:routine(ms_now)
    self.abt_sys:update_modify(ms_now) -- 定时计算总属性
    self.move:moving(ms_now)
end

return EntityAnimal
