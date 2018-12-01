-- entity_player.lua
-- xzc
-- 2018-11-20

-- 玩家实体，用于在场景显示

local Entity_player = oo.class( nil,... )

function Entity_player:__init(eid,pid)
    self.pid = pid -- 玩家唯一id
    self.eid = eid -- 实体唯一id
    self.et = ET.PLAYER
end

-- 同步来自gw的基础属性
function Entity_player:update_base_info( base )
    for k,v in pairs(base) do self[k] = v end
end

return Entity_player
