-- entity_animal.lua
-- xzc
-- 2019-01-20

-- 能够移动、攻击的实体，用于处理移动、攻击逻辑

local Move = require "modules.move.move"
local Entity = require "modules.entity.entity"

local Entity_animal = oo.class( Entity,... )

function Entity_animal:__init(...)
    Entity.__init(self,...)

    self.move = Move(self)
end

-- 处理客户端请求移动
function Entity_animal:do_move( conn,pkt )
    -- 后端不处理寻路，前端发给后端的路线都是直线的，如果遇到转弯这些则必须分开发送
    -- 后端不严格检测线路是否可行，只会定时检测，遇到不可行走则会拉玩家到对应的位置

    self.move:move_to(pkt.way,pkt.pix_x,pkt.pix_y)
end

return Entity_animal
