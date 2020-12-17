-- aoi算法测试

local Aoi = require "GridAoi"



local pix = 64 -- 一个格子边长64像素
local width = 6400 -- 地图像素宽度(从0开始)
local height = 12800 -- 地图像素高度(从0开始)
local visual_width = 3 * pix -- 视野半宽(像素)
local visual_height = 4 * pix -- 视野半高(像素)

local visual_grid_w = math.ceil((visual_width + pix) / pix)
local visual_grid_h = math.ceil((visual_height + pix) / pix)

local is_valid = true -- 测试性能时不做校验

local entity_info = {}

-- 实体类型(按位表示，因为需要按位筛选)
local INTEREST = 1 -- 第一位表示是否关注其他实体，C++那边定死了，不能占用
local ET_PLAYER = 2 + INTEREST -- 玩家，关注所有实体事件
local ET_NPC = 4 -- npc，关注玩家事件
local ET_MONSTER = 8 -- 怪物，不关注任何事件

local tmp_list = {}
local list_in = {}
local list_out = {}

-- 是否在视野内
local function in_visual_range(et,other)
    local x_range = math.abs(math.floor(other.x / pix) - math.floor(et.x / pix))
    if x_range >= visual_grid_w then return false end

    local y_range = math.abs(math.floor(other.y / pix) - math.floor(et.y / pix))
    if y_range >= visual_grid_h then return false end

    return true
end

-- 获取在自己视野范围内，并且关注事件的实体
local function get_visual_list(et, mask)
    local ev_map = {}
    for id,other in pairs(entity_info) do
        if id ~= et.id and 0 ~= (mask & other.mask)
            and in_visual_range(et,other) then
            ev_map[id] = other
        end
    end

    return ev_map
end


-- 校验list里的实体都在et的视野范围内
local function valid_visual_list(et,list)
    local id_map = {}
    local max = list.n
    for idx = 1,max do
        local id = list[idx]

        assert(et.id ~= id) -- 返回列表不应该包含自己

        -- 返回的实体有效并且关注事件
        local other = entity_info[id]

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

-- 把list和自己视野内的实体进行交叉对比，校验list的正确性
local function valid_visual(et, list, mask)
    -- 校验列表里的实体都在视野范围内
    local id_map = valid_visual_list(et,list)

    -- 校验在视野范围的实体都在列表上
    local visual_ev = get_visual_list(et, mask)
    for id,other in pairs(visual_ev) do
        if not id_map[id] then
            PRINTF("visual fail,id = %d,pos(%d,%d) and id = %d,pos(%d,%s)",
                et.id,et.x,et.y,other.id,other.x,other.y)
            assert(false)
        end

        id_map[id] = nil
    end

    -- 校验不在视野范围内或者不关注事件的实体不要在返回列表上
    if not table.empty(id_map) then
        PRINT("THOSE ID IN LIST BUT NOT IN VISUAL RANGE")
        for id in pairs(id_map) do
            PRINT(id)
        end
        assert(false)
    end
end

-- 校验更新位置时收到实体进入事件的列表
local function valid_in(et, list)
    valid_visual_list(et, list)
end

-- 校验更新位置时收到实体退出事件的列表
local function valid_out(et, list)
    local id_map = {}
    local max = list.n
    for idx = 1,max do
        local id = list[idx]

        assert(et.id ~= id) -- 返回列表不应该包含自己

        -- 返回的实体有效并且关注事件
        local other = entity_info[id]
        assert(other and 0 ~= (other.mask & INTEREST),
            string.format("id is %d",id))

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

-- 对interest_me列表进行校验
local function valid_interest_me(aoi, et)
    aoi:get_interest_me_entity(et.id,tmp_list)
    valid_visual(et,tmp_list, INTEREST)
end

local function enter(aoi, id, x, y, mask)
    local entity = entity_info[id]
    if not entity then
        entity = {}
        entity_info[id] = entity
    end

    entity.id = id
    entity.x  = x
    entity.y  = y

    entity.mask = mask

    -- PRINT(id,"enter pos is",math.floor(x/pix),math.floor(y/pix))
    aoi:enter_entity(id,x,y,mask,tmp_list)
    if is_valid then valid_visual(entity,tmp_list, 0xF) end

    return entity
end

local function update(aoi, id,x,y)
    local entity = entity_info[id]
    assert(entity)

    entity.x  = x
    entity.y  = y

    aoi:update_entity(id,x,y,tmp_list,list_in,list_out)
    -- PRINT("update pos", id, x, y, tmp_list.n, list_in.n, list_out.n )

    if is_valid then
        valid_visual_list(entity,tmp_list)
        valid_in(entity, list_in)
        valid_out(entity, list_out)
        valid_interest_me(aoi, entity)
    end
end

local function exit(aoi, id)
    local entity = entity_info[id]
    assert(entity)

    aoi:exit_entity(id,tmp_list)

    entity_info[id] = nil
    -- PRINT(id,"exit pos is",math.floor(entity.x/pix),math.floor(entity.y/pix))
    if is_valid then valid_visual(entity,tmp_list, INTEREST) end
end



-- 做随机测试
local function random_map(map, max_entity)
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

local function random_test(aoi, max_width, max_height, max_entity, max_random)
    local exit_info = {}
    local mask_opt = { ET_PLAYER, ET_NPC, ET_MONSTER }
    for idx = 1,max_entity do
        local x = math.random(0,max_width)
        local y = math.random(0,max_height)
        local mask_idx = math.random(#mask_opt)
        local et = enter(aoi, idx,x,y,mask_opt[mask_idx])
        if is_valid then valid_interest_me(aoi, et) end
    end

    -- 随机退出、更新、进入
    for _ = 1,max_random do
        local ev = math.random(1,3)
        if 1 == ev then
            local et = random_map(exit_info, max_entity)
            if et then
                local x = math.random(0,max_width)
                local y = math.random(0,max_height)
                local new_et = enter(aoi, et.id,x,y,et.mask)
                exit_info[et.id] = nil
                if is_valid then  valid_interest_me(aoi, new_et) end
            end
        elseif 2 == ev then
            local et = random_map(entity_info, max_entity)
            if et then
                local x = math.random(0,max_width)
                local y = math.random(0,max_height)
                update(aoi, et.id,x,y)
                if is_valid then valid_interest_me(aoi, et) end
            end
        elseif 3 == ev then
            local et = random_map(entity_info, max_entity)
            if et then
                exit(aoi, et.id)
                exit_info[et.id] = et
            end
        end
    end
end

t_describe("test grid aoi", function()
    t_it("base test", function()
        local aoi = Aoi()
        aoi:set_size(width, height, pix)
        aoi:set_visual_range(visual_width, visual_height)

        -- 坐标传入的都是像素，坐标从0开始，所以减1
        local max_width = width - 1
        local max_height = height - 1

        -- 测试进入四个角
        enter(aoi, 99996,0,0,ET_PLAYER)
        enter(aoi, 99997,max_width,0,ET_NPC)
        enter(aoi, 99998,0,max_height,ET_MONSTER)
        enter(aoi, 99999,max_width,max_height,ET_PLAYER)

        aoi:get_all_entity(ET_PLAYER + ET_NPC + ET_MONSTER, tmp_list)
        t_equal(tmp_list.n, table.size(entity_info))
        aoi:get_all_entity(ET_PLAYER, tmp_list)
        t_equal(tmp_list.n, 2)

        aoi:get_entity(ET_PLAYER, tmp_list, 0, 0, 10, 10)
        t_equal(tmp_list.n, 1)
        aoi:get_entity(ET_NPC, tmp_list, 0, 0, 10, 10)
        t_equal(tmp_list.n, 0)

        -- 测试进入视野(地图边界)
        update(aoi, 99996, max_width, max_height)
        update(aoi, 99998, max_width, max_height)
        update(aoi, 99997, max_width, max_height)
        aoi:get_entity(0xF, tmp_list, 0, 0, 10, 10)
        t_equal(tmp_list.n, 0)

        -- 在同一个格子内移动
        -- 这个移动比较特殊，同一个格子表示aoi没有变化，因此list_in和list_out为空
        -- list是interest_me列表而不是整个视野范围内的实体
        update(aoi, 99999, max_width - pix / 2, max_height - pix / 2)
        -- 校验只有另一个玩家对自己感兴趣
        aoi:get_interest_me_entity(99999, tmp_list)
        t_equal(tmp_list.n, 1)
        -- 对于怪物或者npc而言，应该有两个玩家对自己感兴趣
        aoi:get_interest_me_entity(99997, tmp_list)
        t_equal(tmp_list.n, 2)
        local move = aoi:update_entity(99999,max_width,max_height,tmp_list)
        t_assert(not move)
        t_equal(tmp_list.n, 1)
        t_equal(tmp_list[1], 99996)

        -- 测试离开视野(临界值)
        update(aoi, 99996, max_width, max_height - visual_height - pix)

        -- 测试离开其中一个视野的同时进入另一个实体的视野
        update(aoi, 99997, max_width - visual_width * 2 - (pix - 1), max_height)
        update(aoi, 99998, max_width - visual_width, max_height)
        -- 现在99998应该刚好在能在左右看到99997和99999
        aoi:get_visual_entity(99998, 0xF, tmp_list)
        t_equal(tmp_list.n, 3)

        -- 退出场景
        exit(aoi, 99996)
        exit(aoi, 99997)
        exit(aoi, 99998)
        exit(aoi, 99999)

        aoi:get_entity(0xF, tmp_list, 0, 0, max_width, max_height)
        t_equal(tmp_list.n, 0)

        local max_entity = 2000
        local max_random = 50000
        random_test(aoi, max_width, max_height, max_entity, max_random)
    end)

    local aoi = Aoi()
    local max_entity = 2000
    local max_random = 50000
    t_it(string.format(
        "perf test %d entity and %d times random move/exit/enter",
        max_entity, max_random), function()
        is_valid = false
        entity_info = {}

        aoi:set_size(width, height, pix)
        aoi:set_visual_range(visual_width, visual_height)

        -- 坐标传入的都是像素，坐标从0开始，所以减1
        local max_width = width - 1
        local max_height = height - 1

        random_test(aoi, max_width, max_height, max_entity, max_random)
    end)

    local max_query = 1000
    t_it(string.format(
        "query test %d entity and %d times visual range",
        max_entity, max_query), function()
        for _ = 1, max_query do
            for id in pairs(entity_info) do
                aoi:get_visual_entity(id, 0xF, tmp_list)
            end
        end
    end)
end)
