-- tower_aoi_test.lua 灯塔AOI测试
local TowerAoi = require "scene.tower_aoi"

-- local pix = 64 -- 一个格子边长64像素（用于视野计算参考）
local width = 6400 -- 地图像素宽度(从0开始)
local height = 12800 -- 地图像素高度(从0开始)
local tower_size = 256 -- 灯塔管理范围
local visual_diameter = 256 -- 视野直径（与tower_size对齐，确保单灯塔覆盖）

local is_valid = true -- 测试性能时不做校验

local entity_info = {}
local tmp_list = {}

-- 是否在视野内（正方形视野，边长=visual_diameter）
local function in_visual_range(et, other)
    local radius = visual_diameter / 2
    if math.abs(et.x - other.x) > radius then return false end
    if math.abs(et.y - other.y) > radius then return false end
    return true
end

-- 获取在自己视野范围内的实体
local function get_visual_list(et)
    local ev_map = {}
    for id, other in pairs(entity_info) do
        if id ~= et.id and in_visual_range(et, other) then
            ev_map[id] = other
        end
    end
    return ev_map
end

-- 校验list里的实体都在et的视野范围内
local function valid_visual_list(et, list)
    local id_map = {}
    local max = list.n
    for idx = 1, max do
        local id = list[idx]
        assert(et.id ~= id, id) -- 返回列表不应该包含自己

        local other = entity_info[id]
        if not other then
            printf("entity not found in visual list, et=%d, other=%d", et.id, id)
            assert(false)
        end

        if not in_visual_range(et, other) then
            printf("valid_visual_list fail, id=%d, pos(%d,%d) and id=%d, pos(%d,%d)",
                   et.id, et.x, et.y, other.id, other.x, other.y)
            assert(false)
        end

        assert(nil == id_map[id], "duplicate id in visual list: " .. id)
        id_map[id] = true
    end
    return id_map
end

-- 把list和自己视野内的实体进行交叉对比，校验list的正确性
local function valid_visual(et, list)
    local id_map = valid_visual_list(et, list)

    local visual_ev = get_visual_list(et)
    for id, other in pairs(visual_ev) do
        if not id_map[id] then
            printf("valid_visual fail, id=%d, pos(%d,%d) and id=%d, pos(%d,%d)",
                   et.id, et.x, et.y, other.id, other.x, other.y)
            assert(false)
        end
        id_map[id] = nil
    end

    if not table.empty(id_map) then
        print("THOSE ID IN LIST BUT NOT IN VISUAL RANGE")
        for id in pairs(id_map) do print(id) end
        assert(false)
    end
end

local function enter(aoi, id, x, y)
    local entity = entity_info[id]
    if not entity then
        entity = {}
        entity_info[id] = entity
    end

    entity.id = id
    entity.x = x
    entity.y = y

    local obj = {uid = id, x = x, y = y}
    aoi:add(obj)

    if is_valid then
        aoi:get_visual_entity(id, visual_diameter, tmp_list)
        valid_visual(entity, tmp_list)
    end

    return entity
end

local function exit(aoi, id)
    local entity = entity_info[id]
    assert(entity)

    local obj = {uid = id, x = entity.x, y = entity.y}
    aoi:del(obj)

    entity_info[id] = nil
end

local function update(aoi, id, x, y)
    local entity = entity_info[id]
    assert(entity)

    -- 先从旧位置删除
    local old_obj = {uid = id, x = entity.x, y = entity.y}
    aoi:del(old_obj)

    -- 更新位置
    entity.x = x
    entity.y = y

    -- 在新位置添加
    local new_obj = {uid = id, x = x, y = y}
    aoi:add(new_obj)

    if is_valid then
        aoi:get_visual_entity(id, visual_diameter, tmp_list)
        valid_visual(entity, tmp_list)
    end
end

-- 从table中随机一个值
local function random_map(map, max_entity)
    local srand = max_entity / 3
    local idx = 0
    local hit = nil
    for _, et in pairs(map) do
        if not hit then hit = et end
        idx = idx + 1
        if idx >= srand then return et end
    end
    return hit
end

-- 随机退出、更新、进入进行批量测试
local function random_test(aoi, max_width, max_height, max_entity, max_random)
    local exit_info = {}
    for idx = 1, max_entity do
        local x = math.random(0, max_width)
        local y = math.random(0, max_height)
        enter(aoi, idx, x, y)
    end

    for _ = 1, max_random do
        local ev = math.random(1, 3)
        if 1 == ev then
            local et = random_map(exit_info, max_entity)
            if et then
                local x = math.random(0, max_width)
                local y = math.random(0, max_height)
                enter(aoi, et.id, x, y)
                exit_info[et.id] = nil
            end
        elseif 2 == ev then
            local et = random_map(entity_info, max_entity)
            if et then
                local x = math.random(0, max_width)
                local y = math.random(0, max_height)
                update(aoi, et.id, x, y)
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

Test.describe("test tower aoi", function()
    Test.it("base test", function()
        is_valid = true
        entity_info = {}
        local aoi = TowerAoi(tower_size, width, visual_diameter)

        local max_width = width - 1
        local max_height = height - 1

        -- 测试进入四个角
        enter(aoi, 99996, 0, 0)
        enter(aoi, 99997, max_width, 0)
        enter(aoi, 99998, 0, max_height)
        enter(aoi, 99999, max_width, max_height)

        aoi:get_all_entity(tmp_list)
        Test.equal(tmp_list.n, table.size(entity_info))

        -- 测试视野查询
        aoi:get_visual_entity(99996, visual_diameter, tmp_list)
        -- 99996在(0,0)，视野范围内应该没有其他角的实体
        Test.equal(tmp_list.n, 0)

        -- 测试进入视野
        update(aoi, 99996, max_width, max_height)
        aoi:get_visual_entity(99996, visual_diameter, tmp_list)
        -- 现在99996和99997,99998,99999都在右上角附近
        -- 但视野是正方形，边长=visual_diameter=256
        -- 99997在(max_width,0)，99998在(0,max_height)，距离太远看不到
        -- 只有99999在(max_width,max_height)能看到

        -- 退出场景
        exit(aoi, 99996)
        exit(aoi, 99997)
        exit(aoi, 99998)
        exit(aoi, 99999)

        aoi:get_all_entity(tmp_list)
        Test.equal(tmp_list.n, 0)

        -- 随机测试
        local max_entity = 2000
        local max_random = 50000
        random_test(aoi, max_width, max_height, max_entity, max_random)
    end)

    local aoi = nil
    local max_entity = 2000
    local max_random = 10000
    Test.it(string.format(
             "perf test %d entity and %d times random move/exit/enter",
             max_entity, max_random), function()
        is_valid = false
        entity_info = {}
        aoi = TowerAoi(tower_size, width, visual_diameter)

        local max_width = width - 1
        local max_height = height - 1

        random_test(aoi, max_width, max_height, max_entity, max_random)
    end)

    local max_query = 1000
    Test.it(string.format("query visual test %d entity and %d times visual range",
                       max_entity, max_query), function()
        if not aoi then
            aoi = TowerAoi(tower_size, width, visual_diameter)
            local max_width = width - 1
            local max_height = height - 1
            random_test(aoi, max_width, max_height, max_entity, 0)
        end
        for _ = 1, max_query do
            for id in pairs(entity_info) do
                aoi:get_visual_entity(id, visual_diameter, tmp_list)
            end
        end
        Test.print("actually run " .. table.size(entity_info))
    end)
end)
