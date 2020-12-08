-- aoi算法测试

local Aoi = require "GridAoi"



local pix = 64 -- 一个格子边长64像素
local width = 6400 -- 地图像素宽度(从0开始)
local height = 6400 -- 地图像素高度(从0开始)
local visual_width = 3 * pix -- 视野半宽(像素)
local visual_height = 4 * pix -- 视野半高(像素)

local is_valid = true -- 测试性能时不做校验

local entity_info = {}

-- 实体类型
local ET_PLAYER = 1 -- 玩家，关注所有实体事件
local ET_NPC = 2 -- npc，关注玩家事件
local ET_MONSTER = 3 -- 怪物，不关注任何事件

local entity_pack_list = {}

-- 是否在视野内
local function in_visual_range(et,other)
    local x_range = math.abs(other.x - et.x)
    local y_range = math.abs(other.y - et.y)

    if x_range > visual_width + pix then return false end
    if y_range > visual_height + pix then return false end

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


-- 校验触发事件时返回的实体列表
local function raw_valid_ev(et,list)
    local id_map = {}
    local max = list.n
    for idx = 1,max do
        local id = list[idx]

        assert(et.id ~= id) -- 返回列表不应该包含自己

        -- 返回的实体有效并且关注事件
        local other = entity_info[id]
        assert(other and 0 ~= other.event,string.format("id is %d",id))

        -- 校验视野范围
        if not in_visual_range(et,other) then
            PRINTF("range fail,id = %d,pos(%d,%d) and id = %d,pos(%d,%s)",
                et.id,et.x,et.y,other.id,other.x,other.y)
            assert(false)
        end

        assert( nil == id_map[id] ) -- 校验返回的实体不会重复
        id_map[id] = true
    end

    return id_map
end

local function valid_ev(et,list)
    local id_map = raw_valid_ev(et,list)
    -- 校验在视野范围的实体都在列表上
    local visual_ev = get_visual_ev(et)
    for id,other in pairs(visual_ev) do
        if not id_map[id] then
            PRINTF("visual fail,id = %d,pos(%d,%d) and id = %d,pos(%d,%s)",
                et.id,et.x,et.y,other.id,other.x,other.y)
            assert(false)
        end

        id_map[id] = nil
    end

    -- 校验不在视野范围内或者不关注事件的实体不要在返回列表上
    assert( table.empty(id_map) )
end

-- 校验更新位置时收到实体进入事件的列表
local function valid_in(et,list)
    raw_valid_ev(et,list)
end

-- 校验更新位置时收到实体退出事件的列表
local function valid_out(et,list)
    local id_map = {}
    local max = list.n
    for idx = 1,max do
        local id = list[idx]

        assert(et.id ~= id) -- 返回列表不应该包含自己

        -- 返回的实体有效并且关注事件
        local other = entity_info[id]
        assert(other and 0 ~= other.event,string.format("id is %d",id))

        -- 校验视野范围
        if in_visual_range(et,other) then
            PRINTF("range fail,id = %d,pos(%d,%d) and id = %d,pos(%d,%s)",
                et.id,et.x,et.y,other.id,other.x,other.y)
            assert(false)
        end

        assert( nil == id_map[id] ) -- 校验返回的实体不会重复
        id_map[id] = true
    end
end

-- 对watch_me列表进行校验
local function valid_watch_me(aoi, et)
    -- 检验watch列表
    aoi:get_watch_me_entitys(et.id,entity_pack_list)
    valid_ev(et,entity_pack_list)
end

local function enter(aoi, id,x,y,type,event)
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

    -- PRINT(id,"enter pos is",math.floor(x/pix),math.floor(y/pix))
    aoi:enter_entity(id,x,y,type,event,entity_pack_list)
    if is_valid then valid_ev(entity,entity_pack_list) end

    return entity
end

local function update(aoi, id,x,y)
    local entity = entity_info[id]
    assert(entity)

    entity.x  = x
    entity.y  = y

    local list_in = {}
    local list_out = {}
    -- PRINT(id,"new pos is",math.floor(x/pix),math.floor(y/pix))
    aoi:update_entity(id,x,y,entity_pack_list,list_in,list_out)

    if is_valid then
        raw_valid_ev(entity,entity_pack_list)
        valid_in(entity,list_in)
        valid_out(entity,list_out)
    end
end

local function exit(aoi, id)
    local entity = entity_info[id]
    assert(entity)

    aoi:exit_entity(id,entity_pack_list)

    entity_info[id] = nil
    -- PRINT(id,"exit pos is",math.floor(entity.x/pix),math.floor(entity.y/pix))
    if is_valid then valid_ev(entity,entity_pack_list) end
end



-- 做随机测试
local max_entity = 2000
local max_random = 5000
local function random_map(map)
    -- 随机大概会达到一个平衡的,随便取一个值
    local srand = max_entity/3

    local idx = 0
    local hit = nil
    for _, et in pairs(map) do
        if not hit then hit = et end
        idx = idx + 1
        if idx >= srand then return et end
    end

    return hit
end

local function random_test(aoi, max_width, max_height)
    local exit_info = {}
    for idx = 1,max_entity do
        local x = math.random(0,max_width)
        local y = math.random(0,max_height)
        local entity_type = math.random(1,3)
        local event = math.random(0,1)
        local et = enter(aoi, idx,x,y,entity_type,event)
        valid_watch_me(aoi, et)
    end

    -- 随机退出、更新、进入
    for _ = 1,max_random do
        local ev = math.random(1,3)
        if 1 == ev then
            local et = random_map(exit_info)
            if et then
                local x = math.random(0,max_width)
                local y = math.random(0,max_height)
                local new_et = enter(aoi, et.id,x,y,et.type,et.event)
                exit_info[et.id] = nil
                valid_watch_me(aoi, new_et)
            end
        elseif 2 == ev then
            local et = random_map(entity_info)
            if et then
                local x = math.random(0,max_width)
                local y = math.random(0,max_height)
                update(aoi, et.id,x,y)
                valid_watch_me(aoi, et)
            end
        elseif 3 == ev then
            local et = random_map(entity_info)
            if et then
                exit(aoi, et.id)
                exit_info[et.id] = et
            end
        end
    end
end

t_describe("grid aoi", function()
    t_it("base test", function()
        local aoi = Aoi()
        aoi:set_size(width, height, pix)
        aoi:set_visual_range(visual_width, visual_height)

        -- 坐标传入的都是像素，坐标从0开始，所以减1
        local max_width = width - 1
        local max_height = height - 1
        enter(aoi, 99996,0,0,ET_PLAYER,1)
        enter(aoi, 99997,max_width,0,ET_NPC,1)
        enter(aoi, 99998,0,max_height,ET_MONSTER,0)
        enter(aoi, 99999,max_width,max_height,ET_PLAYER,1)

        update(aoi, 99997,0,max_height) -- 进入同一个格子
        update(aoi, 99998,max_width,max_height) -- 离开有人的格子
        update(aoi, 99997,max_width,max_height) -- 三个实体在同一个格子
        update(aoi, 99999,max_width - 1,max_height - 1) -- 测试视野范围内移动

        exit(aoi, 99996)
        exit(aoi, 99997)
        exit(aoi, 99998)
        exit(aoi, 99999)

        random_test(aoi, max_width, max_height)
    end)

    t_it("perf test", function()
    end)
end)
