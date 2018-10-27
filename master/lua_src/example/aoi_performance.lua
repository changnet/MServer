local Aoi = require "Aoi"

local aoi = Aoi()

local width = 64
local height = 64
aoi:set_size(width*64,height*64,64)
aoi:set_visual_range(3,4)

local entity_info = {}

-- 实体类型
local ET_PLAYER = 1 -- 玩家，关注所有实体事件
local ET_NPC = 2 -- npc，关注玩家事件
local ET_MONSTER = 3 -- 怪物，不关注任何事件

local entity_pack_list = {}

-- 校验所有实体数据
local function valid(list)
    -- 校验有没有重复
    -- 校验所在格子
    -- 校验底层坐标和脚本坐标是否一致
    local times = {}

    for _,id in pairs(list) do

    end
end

-- 校验触发事件时返回的实体列表
local function valid_ev()
end

-- 校验watch_me列表
local function valid_watch_me()
end

local function enter(id,x,y,type,event)
    local entity = entity_info[id]
    if not entity then
        entity = {}
        entity_info[id] = entity
    end

    entity.id = id
    entity.x  = x
    entity.y  = y

    entity.type = type
    entity.event = event

    aoi:enter_entity(id,x,y,type,event,entity_pack_list)
    valid_ev(entity,entity_pack_list)
end

local function update(id,x,y)
    local entity = entity_info[id]
    assert(entity)

    entity.x  = x
    entity.y  = y

    local list_in = {}
    local list_out = {}
    aoi:update_entity(id,x,y,entity_pack_list,list_in,list_out)
end

local function exit(id)
    local entity = entity_info[id]
    assert(entity)

    aoi:exit_entity(id,entity_pack_list)

    entity_info[id] = nil
    valid_ev(entity,entity_pack_list)
end

enter(99996,0,0,ET_PLAYER,1)
enter(99997,width,0,ET_NPC,1)
enter(99998,0,height,ET_MONSTER,0)
enter(99999,width,height,ET_PLAYER,1)
