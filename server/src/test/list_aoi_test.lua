-- list_aoi_test.lua 十字链表AOI测试
-- xzc
-- 2021-01-01

local ListAoi = require "ListAoi"

-- 默认使用左手坐标系，故2D地图只有x、z轴，没有y轴

local MAX_X = 6400 -- 地图像素z轴最大值(从0开始)
local MAX_Y = 19200 -- 地图像素z轴最大值(从0开始)
local MAX_Z = 12800 -- 地图像素z轴最大值(从0开始)

-- 实体类型(按位表示，因为需要按位筛选)
local INTEREST = 1 -- 第一位表示是否关注其他实体，C++那边定死了，不能占用
local ET_PLAYER = 2 + INTEREST -- 玩家，关注所有实体事件
local ET_NPC = 4 -- npc，不关注任何事件
local ET_MONSTER = 8 -- 怪物，不关注任何事件

local V_PLAYER = 500 -- 玩家的视野

local is_valid -- 是否校验结果，perf时不校验
local is_use_y -- 是否使用y轴
local tmp_list = {}
local list_in = {}
local list_out = {}
local entity_info = {}

-- 是否在视野内
local function in_visual_range(et,other)
    local visual = et.visual
    if visual < math.abs(et.x - other.x) then return false end

    if is_use_y then
        if visual < math.abs(et.y - other.y) then return false end
    end

    if visual < math.abs(et.z - other.z) then return false end

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
    -- 校验list列表里的实体都在视野范围内
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

-- 测试实体进入aoi并校验结果
local function enter(aoi, id, x, y, z, visual, mask)
    local entity = entity_info[id]
    if not entity then
        entity = {}
        entity_info[id] = entity
    end

    entity.id = id
    entity.x  = x
    entity.y  = y
    entity.z = z
    entity.visual = visual
    entity.mask = mask

    -- PRINT(id,"enter pos is",math.floor(x/pix),math.floor(y/pix))
    aoi:enter_entity(id,x,y,z, visual, mask,tmp_list)
    if is_valid then valid_visual(entity,tmp_list, 0xF) end

    return entity
end

t_describe("list aoi test", function()
    t_it("base list aoi test", function()
        is_valid = true
        is_use_y = true
        local aoi = ListAoi()

        -- 坐标传入的都是像素，坐标从0开始，所以减1

        -- 测试进入临界值
        enter(aoi, 99991, 0, 0, 0, V_PLAYER, ET_PLAYER)
        enter(aoi, 99992, MAX_X, 0, 0, 0, ET_NPC)
        enter(aoi, 99993, 0, MAX_Y, 0, 0, ET_MONSTER)
        enter(aoi, 99994, 0, 0, MAX_Z, V_PLAYER, ET_PLAYER)
        enter(aoi, 99995, MAX_X, MAX_Y, MAX_Z, V_PLAYER, ET_PLAYER)
    end)

    t_it("list aoi perf", function()
        is_valid = false
        is_use_y = true
        local aoi = ListAoi()
    end)

    t_it("list aoi perf no y", function()
        is_valid = false
        is_use_y = false
        local aoi = ListAoi()

        aoi:use_y(false)
    end)

    t_it("list aoi query perf", function()
        is_valid = false
        is_use_y = false
        local aoi = ListAoi()

        aoi:use_y(false)
    end)

    t_it("list aoi visual change perf", function()
        is_valid = false
        is_use_y = false
        local aoi = ListAoi()

        aoi:use_y(false)
    end)
end)
