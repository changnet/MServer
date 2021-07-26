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

local V_PLAYER = 256 -- 玩家的视野半径

local is_valid -- 是否校验结果，perf时不校验
local is_use_y -- 是否使用y轴
local is_use_history -- 是否记录历史

local tmp_list = {}
local list_me_in = {}
local list_other_in = {}
local list_me_out = {}
local list_other_out = {}

local entity_info = {}
local exit_info = {}

-- 是否在视野内
local function in_visual_range(et, other)
    local visual = et.visual
    if visual <= 0 then return false end
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
    if et.visual <= 0 then return ev_map end

    for id, other in pairs(entity_info) do
        if id ~= et.id and 0 ~= (mask & other.mask) and
            in_visual_range(et, other) then ev_map[id] = other end
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

        -- 返回的实体有效并且关注事件
        local other = entity_info[id]

        -- 校验视野范围
        if not in_visual_range(et, other) then
            printf("valid_visual_list fail,id = %d,\z
                pos(%d,%d,%d) and id = %d,pos(%d,%d,%d)", et.id, et.x, et.y,
                   et.z, other.id, other.x, other.y, other.z)
            assert(false)
        end

        assert(nil == id_map[id]) -- 校验返回的实体不会重复
        id_map[id] = true
    end

    return id_map
end

-- 校验et在list里的实体视野范围内
local function valid_other_visual_list(et, list)
    local id_map = {}
    local max = list.n
    for idx = 1, max do
        local id = list[idx]

        assert(et.id ~= id) -- 返回列表不应该包含自己

        -- 返回的实体有效并且关注事件
        local other = entity_info[id]
        if not other then
            print("entity not found", et.id, id)
            assert(false)
        end

        -- 校验视野范围
        if not in_visual_range(other, et) then
            printf("range fail,id = %d,pos(%d,%d,%d) and id = %d,pos(%d,%s,%d)",
                   et.id, et.x, et.y, et.z, other.id, other.x, other.y, other.z)
            assert(false)
        end

        if (id_map[id]) then
            printf("id duplicate in %d", et.id)
            vd(list)
        end

        assert(nil == id_map[id]) -- 校验返回的实体不会重复
        id_map[id] = true
    end

    return id_map
end

-- 把list和自己视野内的实体进行交叉对比，校验list的正确性
local function valid_visual(et, list_me, list_other, mask)
    -- 校验list列表里的实体都在视野范围内
    local id_map = valid_visual_list(et, list_me)

    -- 校验在视野范围的实体都在列表上
    local visual_et = get_visual_list(et, mask)
    for id, other in pairs(visual_et) do
        if not id_map[id] then
            printf("valid_visual fail,id = %d,\z
                pos(%d,%d,%d) and id = %d,pos(%d,%s,%d)", et.id, et.x, et.y,
                   et.z, other.id, other.x, other.y, other.z)
            assert(false)
        end

        id_map[id] = nil
    end

    -- 校验不在视野范围内或者不关注事件的实体不要在返回列表上
    if not table.empty(id_map) then
        print("THOSE ID IN LIST BUT NOT IN VISUAL RANGE")
        for id in pairs(id_map) do print(id) end
        assert(false)
    end

    -- 校验自己在别人的视野范围内
    valid_other_visual_list(et, list_other)
end

-- 校验更新位置时收到实体进入事件的列表
local function valid_in(et, list)
    valid_visual_list(et, list)
end

-- 校验自己进入别人的视野
local function valid_other_in(et, list)
    -- et进入了list中实体的视野范围内
    valid_other_visual_list(et, list)
end

-- 校验更新位置时收到实体退出事件的列表
local function valid_out(et, list)
    local id_map = {}
    local max = list.n
    for idx = 1, max do
        local id = list[idx]

        assert(et.id ~= id) -- 返回列表不应该包含自己

        -- 返回的实体有效并且关注事件
        local other = entity_info[id]
        assert(other)

        -- 校验视野范围
        if in_visual_range(et, other) then
            printf("valid_out fail,id = %d,\z
                pos(%d,%d,%d) and id = %d,pos(%d,%s,%d)", et.id, et.x, et.y,
                   et.z, other.id, other.x, other.y, other.z)
            assert(false)
        end

        assert(nil == id_map[id]) -- 校验返回的实体不会重复
        id_map[id] = true
    end
end

-- 校验et退出list中实体的视野范围
local function valid_other_out(et, list)
    local id_map = {}
    local max = list.n
    for idx = 1, max do
        local id = list[idx]

        assert(et.id ~= id) -- 返回列表不应该包含自己

        -- 返回的实体有效并且关注事件
        local other = entity_info[id]
        assert(other and 0 ~= (other.mask & INTEREST),
               string.format("id is %d", id))

        -- 校验视野范围
        if in_visual_range(other, et) then
            printf("valid_other_out fail,id = %d,\z
                pos(%d,%d,%d) and id = %d,pos(%d,%s,%d)", et.id, et.x, et.y,
                   et.z, other.id, other.x, other.y, other.z)
            assert(false)
        end

        assert(nil == id_map[id]) -- 校验返回的实体不会重复
        id_map[id] = true
    end
end

-- 对interest_me列表进行校验
local function valid_interest_me(aoi, et)
    aoi:get_visual_entity(et.id, INTEREST, list_me_in)
    aoi:get_interest_me_entity(et.id, list_other_in)
    valid_visual(et, list_me_in, list_other_in, INTEREST)
end

-- 测试实体进入aoi并校验结果
local function enter(aoi, id, x, y, z, visual, mask)
    local entity = entity_info[id]
    if not entity then
        entity = {}
        entity_info[id] = entity
    end

    entity.id = id
    entity.x = x
    entity.y = y
    entity.z = z
    entity.visual = visual
    entity.mask = mask

    exit_info[id] = nil

    -- print(id,"enter pos is",math.floor(x/pix),math.floor(y/pix))
    aoi:enter_entity(id, x, y, z, visual, mask, list_me_in, list_other_in)
    if is_valid then
        valid_visual(entity, list_me_in, list_other_in, 0xF)
        valid_interest_me(aoi, entity)
    end

    return entity
end

-- 更新实体位置
local function update(aoi, id, x, y, z)
    local entity = entity_info[id]
    assert(entity)

    entity.x = x
    entity.y = y
    entity.z = z

    aoi:update_entity(id, x, y, z, list_me_in, list_other_in, list_me_out,
                      list_other_out)
    if is_valid then
        valid_in(entity, list_me_in)
        valid_other_in(entity, list_other_in)
        valid_out(entity, list_me_out)
        valid_other_out(entity, list_other_out)
        valid_interest_me(aoi, entity)
    end
end

local function exit(aoi, id)
    local entity = entity_info[id]
    assert(entity)

    aoi:exit_entity(id, tmp_list)

    entity_info[id] = nil
    exit_info[id] = entity
    if is_valid then valid_other_visual_list(entity, tmp_list) end
end

-- 从table中随机一个值
local function random_map(map, max_entity)
    -- 随机大概会达到一个平衡的,随便取一个值
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

local history = {}
local function append_history(action, id, x, y, z, v, mask)
    if not is_use_history then return end

    table.insert(history, {
        action = action,
        id = id,
        x = x,
        y = y,
        z = z,
        v = v,
        mask = mask
    })
end

-- luacheck:ignore save_history
-- 调试bug用
local function save_history()
    local json = require "lua_parson"

    printf("%d history action save !", #history)
    json.encode_to_file(history, "list_aoi_his.json")
end

-- luacheck:ignore run_history
local function run_history(load)
    local json = require "lua_parson"
    if load then history = json.decode_from_file("list_aoi_his.json") end

    printf("%d history action load !", #history)

    is_valid = true
    is_use_y = true
    local aoi = ListAoi()
    aoi:set_index(400, MAX_X)
    for idx, his in ipairs(history) do
        if true then
            local action = his.action
            if true then
                printf("%d %s id = %d, x = %d, y = %d, z = %d, v = %d", idx,
                       action, his.id, his.x or -1, his.y or -1, his.z or -1,
                       his.v or -1)
            end

            -- 根据bug条件做不同处理
            if idx == 2590 then
                local entity = entity_info[his.id]

                entity.x = his.x
                entity.y = his.y
                entity.z = his.z
                aoi:get_interest_me_entity(his.id, list_other_in)
                for n = 1, list_other_in.n do
                    print(list_other_in[n])
                end
                print("=======================")
                aoi:update_entity(his.id, his.x, his.y, his.z, list_me_in,
                                  list_other_in, list_me_out, list_other_out)
                -- aoi:dump()
                aoi:get_interest_me_entity(his.id, list_other_in)
                for n = 1, list_other_in.n do
                    print(list_other_in[n])
                end
                valid_interest_me(aoi, entity_info[his.id])
                return
            end

            if "ENTER" == action then
                enter(aoi, his.id, his.x, his.y, his.z, his.v, his.mask)
            elseif "UPDATE" == action then
                update(aoi, his.id, his.x, his.y, his.z)
            elseif "EXIT" == action then
                exit(aoi, his.id)
            else
                assert(false)
            end

            if idx == 2007 or idx == 2008 then
                assert(aoi:valid_dump(true))
            end
        end
    end
    printf("run done %d - %d", table.size(entity_info), table.size(exit_info))
end

-- 随机退出、更新、进入进行批量测试
local function random_test(aoi, max_x, max_y, max_z, max_entity, max_random)
    local mask_opt = {ET_PLAYER, ET_NPC, ET_MONSTER}
    for idx = 1, max_entity do
        local x = math.random(0, max_x)
        local y = math.random(0, max_y)
        local z = math.random(0, max_z)
        local mask = mask_opt[math.random(#mask_opt)]

        local visual = 0
        if ET_PLAYER == mask then visual = V_PLAYER end
        append_history("ENTER", idx, x, y, z, visual, mask)
        enter(aoi, idx, x, y, z, visual, mask)
    end

    -- 随机退出、更新、进入
    for _ = 1, max_random do
        local ev = math.random(1, 3)
        if 1 == ev then
            local et = random_map(exit_info, max_entity)
            if et then
                local x = math.random(0, max_x)
                local y = math.random(0, max_y)
                local z = math.random(0, max_z)
                local visual = 0
                if ET_PLAYER == et.mask then visual = V_PLAYER end
                append_history("ENTER", et.id, x, y, z, visual, et.mask)
                enter(aoi, et.id, x, y, z, visual, et.mask)
            end
        elseif 2 == ev then
            local et = random_map(entity_info, max_entity)
            if et then
                local x = math.random(0, max_x)
                local y = math.random(0, max_y)
                local z = math.random(0, max_z)
                append_history("UPDATE", et.id, x, y, z)
                update(aoi, et.id, x, y, z)
            end
        elseif 3 == ev then
            local et = random_map(entity_info, max_entity)
            if et then
                append_history("EXIT", et.id)
                exit(aoi, et.id)
            end
        end
    end

    if is_valid then assert(aoi:valid_dump(false)) end
end

t_describe("list aoi test", function()
    t_it("list_aoi_bug", function()
        is_valid = true
        is_use_y = true
        local aoi = ListAoi()
        aoi:set_index(400, MAX_X)

        -- 当移动667时，x轴左移刚好跨过691视野左边界，y轴左移出视野，需要C++那边
        -- 处理实体重复标记
        local et99981 = enter(aoi, 691, 2395, 17853, 570, 256, ET_PLAYER)
        enter(aoi, 667, 2446, 17851, 319, 256, ET_PLAYER)
        update(aoi, 667, 2260, 1581, 9303)
        valid_interest_me(aoi, et99981)
    end)
    t_it("base list aoi test", function()
        entity_info = {}
        exit_info = {}
        is_valid = true
        is_use_y = true
        local aoi = ListAoi()

        aoi:set_index(400, MAX_X)

        -- 测试进入临界值,坐标传入的都是像素，坐标从0开始，所以减1
        enter(aoi, 99991, 0, 0, 0, V_PLAYER, ET_PLAYER)
        enter(aoi, 99992, MAX_X - 1, 0, 0, 0, ET_NPC)
        enter(aoi, 99993, 0, MAX_Y - 1, 0, 0, ET_MONSTER)
        enter(aoi, 99994, 0, 0, MAX_Z - 1, V_PLAYER, ET_PLAYER)
        enter(aoi, 99995, MAX_X - 1, MAX_Y - 1, MAX_Z - 1, V_PLAYER, ET_PLAYER)

        aoi:get_all_entity(ET_PLAYER + ET_NPC + ET_MONSTER, tmp_list)
        t_equal(tmp_list.n, table.size(entity_info))
        aoi:get_all_entity(ET_PLAYER, tmp_list)
        t_equal(tmp_list.n, 3)

        aoi:get_entity(ET_PLAYER, tmp_list, 0, 0, 0, 0, 0, 0)
        t_equal(tmp_list.n, 1)
        aoi:get_entity(ET_NPC, tmp_list, 0, MAX_X / 2, 0, MAX_Y - 1, 0,
                       MAX_Z - 1)
        t_equal(tmp_list.n, 0)

        update(aoi, 99991, MAX_X - 1 - V_PLAYER, MAX_Y - 1 - V_PLAYER,
               MAX_Z - 1 - V_PLAYER)

        -- 进入其他人视野
        update(aoi, 99992, MAX_X - 1, MAX_Y - 1, MAX_Z - 1)
        update(aoi, 99993, MAX_X - 1, MAX_Y - 1, MAX_Z - 1)
        update(aoi, 99994, MAX_X - 1, MAX_Y - 1, MAX_Z - 1)

        -- 这个移动应该不需要移动链表
        update(aoi, 99991, MAX_X - V_PLAYER, MAX_Y - V_PLAYER, MAX_Z - V_PLAYER)

        aoi:get_entity(ET_PLAYER + ET_NPC + ET_MONSTER, tmp_list, 0, MAX_X - 1,
                       0, MAX_Y - 1, 0, MAX_Z - 1)
        t_equal(tmp_list.n, 5)

        -- 当左右视野坐标和实体坐标一致时，保证视野包含实体
        -- 这个只能通过aoi:dump()来自己看了

        -- 视野不一样时，两个实体能否相互看到
        local v = V_PLAYER * 2
        local et6 = enter(aoi, 99996, MAX_X - 1 - v, MAX_Y - 1 - v,
                          MAX_Z - 1 - v, v, ET_PLAYER)
        aoi:get_visual_entity(99996, 0xF, tmp_list)
        t_equal(tmp_list.n, 5)
        aoi:get_visual_entity(99995, 0xF, tmp_list)
        t_equal(tmp_list.n, 4)

        -- 视野增大(现在能看到99996了)
        aoi:update_visual(99995, v, tmp_list)
        t_equal(tmp_list.n, 1)
        t_equal(tmp_list[1], 99996)
        aoi:get_visual_entity(99995, 0xF, tmp_list)
        t_equal(tmp_list.n, 5)

        -- 视野缩小(现在看不到99996了)
        aoi:update_visual(99995, V_PLAYER, nil, tmp_list)
        t_equal(tmp_list.n, 1)
        t_equal(tmp_list[1], 99996)
        aoi:get_visual_entity(99995, 0xF, tmp_list)
        t_equal(tmp_list.n, 4)
        -- 视野为0
        aoi:update_visual(99996, 0, nil, tmp_list)
        t_equal(tmp_list.n, 5)
        aoi:get_visual_entity(99996, 0xF, tmp_list)
        t_equal(tmp_list.n, 0)
        -- 视野从0到不为0
        aoi:update_visual(99996, V_PLAYER * 2, tmp_list)
        t_equal(tmp_list.n, 5)
        aoi:get_visual_entity(99996, 0xF, tmp_list)
        t_equal(tmp_list.n, 5)

        -- 更新位置时，当一个实体的实体指针、右视野指针依次在链表上移过另一个实体的
        -- 视野左边界，另一个实体本身时，mark标记需要特殊处理
        enter(aoi, 99997, et6.x - 1, et6.y - 1, et6.z - 1, v, ET_PLAYER)
        update(aoi, 99997, 1, 1, 1)

        -- 插入一个视野范围大到包含另一个实体(99996)的视野左右边界的实体
        -- 这个要在C++那边处理id是否重复
        enter(aoi, 99998, et6.x, et6.y, et6.z, V_PLAYER * 10, ET_PLAYER)

        -- 所有实体退出，保证链表为空
        exit(aoi, 99991)
        exit(aoi, 99992)
        exit(aoi, 99993)
        exit(aoi, 99994)
        exit(aoi, 99995)
        exit(aoi, 99996)
        exit(aoi, 99997)
        exit(aoi, 99998)
        aoi:get_entity(ET_PLAYER + ET_NPC + ET_MONSTER, tmp_list, 0, MAX_X - 1,
                       0, MAX_Y - 1, 0, MAX_Z - 1)
        t_equal(tmp_list.n, 0)

        -- 随机测试
        local max_entity = 2000
        local max_random = 50000
        is_use_history = true
        random_test(aoi, MAX_X - 1, MAX_Y - 1, MAX_Z - 1, max_entity, max_random)
    end)

    -- 如果随机测试出现一些不好重现的问题，可以把整个过程记录下来，再慢慢排除
    -- t_it("base list aoi history", function()
    --     entity_info = {}
    --     exit_info = {}

    --     -- save_history()
    --     run_history(true)
    -- end)

    local max_entity = 2000
    local max_random = 50000
    t_it(string.format(
             "perf test no_y(more index) %d entity and %d times random M/E/E",
             max_entity, max_random), function()
        entity_info = {}
        exit_info = {}

        is_valid = false
        is_use_y = false
        local aoi_no_y = ListAoi()

        aoi_no_y:set_index(100, MAX_X)

        aoi_no_y:use_y(false)
        is_use_history = false
        random_test(aoi_no_y, MAX_X - 1, MAX_Y - 1, MAX_Z - 1, max_entity,
                    max_random)
    end)

    t_it(string.format(
             "perf test 1 index %d entity and %d times random move/exit/enter",
             max_entity, max_random), function()
        entity_info = {}
        exit_info = {}

        is_valid = false
        is_use_y = false
        local aoi_no_y = ListAoi()

        aoi_no_y:set_index(1, MAX_X)

        aoi_no_y:use_y(false)
        is_use_history = false
        random_test(aoi_no_y, MAX_X - 1, MAX_Y - 1, MAX_Z - 1, max_entity,
                    max_random)
    end)

    -- 下面这几个aoi共用一个aoi和entity_info之类的
    local share_aoi
    t_it(string.format(
             "perf test %d entity and %d times random move/exit/enter",
             max_entity, max_random), function()
        is_valid = false
        is_use_y = true
        entity_info = {}
        exit_info = {}

        share_aoi = ListAoi()
        share_aoi:set_index(400, MAX_X)

        is_use_history = false
        random_test(share_aoi, MAX_X - 1, MAX_Y - 1, MAX_Z - 1, max_entity,
                    max_random)
    end)

    local max_query = 1000
    t_it(string.format("query visual test %d entity and %d times visual range",
                       max_entity, max_query), function()
        for _ = 1, max_query do
            for id in pairs(entity_info) do
                share_aoi:get_visual_entity(id, 0xF, tmp_list)
            end
        end
        t_print("actually run " .. table.size(entity_info))
    end)

    local max_change_visual = 1000
    t_it(
        string.format("change visual test %d entity and %d times visual range",
                      max_entity, max_change_visual), function()

            local cnt = 0
            for _ = 1, max_change_visual do
                for id, et in pairs(entity_info) do
                    if (et.visual > 0) then
                        cnt = cnt + 1
                        share_aoi:update_visual(id,
                                                V_PLAYER * math.random(1, 5),
                                                list_me_in, list_me_out)
                    end
                end
            end
            t_print("actually run " .. cnt)
        end)
end)
