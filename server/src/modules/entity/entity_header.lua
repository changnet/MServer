-- entity_header.lua
-- xzc
-- 2018-12-01

-- 实体宏定义

-- entity type 实体类型，按位表示，底层aoi那边方便筛选
ET =
{
    NPC = 1,  -- npc，不可攻击
    PLAYER = 2, -- 玩家
    MONSTER = 4, -- 怪物
}

return ET
