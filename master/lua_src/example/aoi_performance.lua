local Aoi = require "Aoi"

local aoi = Aoi()

local pix = 64 -- 一个格子边长64像素
local width = 64 -- 地图格子宽度
local height = 64 -- 地图格子高度
local visual_width = 3 -- 视野宽度格子数
local visual_height = 4 -- 视野高度格子数

aoi:set_size(width*pix,height*pix,pix)
aoi:set_visual_range(visual_width,visual_height)

local entity_info = {}

-- 实体类型
local ET_PLAYER = 1 -- 玩家，关注所有实体事件
local ET_NPC = 2 -- npc，关注玩家事件
local ET_MONSTER = 3 -- 怪物，不关注任何事件

local entity_pack_list = {}

-- 是否在视野内
local function in_visual_range(et,other)
    if math.abs(other.x - et.x) > visual_width*pix then return false end
    if math.abs(other.y - et.y) > visual_height*pix then return false end

    return true
end

-- 获取在自己视野范围内，并且关注事件的实体
local function get_visual_ev(et)
    local ev_map = {}
    for id,other in pairs(entity_info) do
        if id ~= et.id and 0 ~= other.event and in_visual_range(et,other) then
            ev_map[id] = other
        end
    end

    return ev_map
end

-- 校验所有实体数据
local function valid(list)
    -- 校验有没有重复
    -- 校验所在格子
    -- 校验底层坐标和脚本坐标是否一致(检验底层指针正确性)
    local times = {}

    for _,id in pairs(list) do

    end
end

-- 校验触发事件时返回的实体列表
local function valid_ev(et,list)
    local id_map = {}
    local max = list.n
    for idx = 1,max do
        id = list[idx]

        assert(et.id ~= id) -- 返回列表不应该包含自己

        -- 返回的实体有效并且关注事件
        local other = entity_info[id]
        assert(other and 0 ~= other.event,string.format("id is %d",id))

        -- 校验视野范围
        assert(in_visual_range(et,other))

        assert( nil == id_map[id] ) -- 校验返回的实体不会重复
        id_map[id] = true
    end

    -- 校验在视野范围的实体都在列表上
    local visual_ev = get_visual_ev(et)
    for id,other in pairs(visual_ev) do
        assert( nil ~= id_map[id] )
        id_map[id] = nil
    end

    -- 校验不在视野范围内或者不关注事件的实体不要在返回列表上
    assert( table.empty(id_map) )
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

-- 坐标传入的都是像素
local max_width = width*pix
local max_heigth = height*pix
enter(99997,max_width,0,ET_NPC,1)
enter(99998,0,max_heigth,ET_MONSTER,0)
enter(99999,max_width,max_heigth,ET_PLAYER,1)

update(99997,0,max_heigth) -- 进入同一个格子
update(99998,max_width,max_heigth) -- 离开有人的格子
update(99997,max_width,max_heigth) -- 三个实体在同一个格子
update(99999,max_width - 1,max_heigth - 1) -- 测试视野范围内移动

exit(99997)
exit(99998)
exit(99999)

-- 上面做一些临界测试，下面开始做随机测试

local max_entity = 1000
local exit_info = {}


for idx = 1,max_entity do
    local x = math.random(0,max_width)
    local y = math.random(0,max_heigth)
    local entity_type = math.random(1,3)
    local event = math.random(0,1)
    enter(idx,x,y,entity_type,event)
end

-- 随机退出、更新、进入

-- 对全地图实体进行校验
for id,entity in pairs(entity_info) do
    -- 检验watch列表
    ao:get_watch_me_entitys(id,entity_pack_list)
    valid_ev(et,entity_pack_list)

    -- 核对坐标、类型、事件
end