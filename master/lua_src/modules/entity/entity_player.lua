-- entity_player.lua
-- xzc
-- 2018-11-20

-- 玩家实体，用于在场景显示

local Attribute_sys = require "modules.attribute.attribute_sys"

local Entity_player = oo.class( nil,... )

function Entity_player:__init(eid,pid)
    self.pid = pid -- 玩家唯一id
    self.eid = eid -- 实体唯一id
    self.et = ET.PLAYER

    self.abt_sys = Attribute_sys()
end

-- 同步来自gw的基础属性
function Entity_player:update_base_info( base )
    for k,v in pairs(base) do self[k] = v end
end

-- 同步来自gw的玩家战斗属性
function Entity_player:update_battle_abt( abt_list )
    local sys = self.abt_sys:get_sys( ABTSYS.SYNC )

    sys:clear()
    for idx = 1,#abt_list,2 do
        sys:push_one( abt_list[idx],abt_list[idx+1] )
    end

    vd(self.abt_sys)
end

return Entity_player
