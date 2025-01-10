-- 排行榜算法测试
--[[
这套东西最早是在C++做的。数据是放在脚本，排序C++来做，通过id来关联，后来发现在脚本做就
能满足需求，就从C++迁移过来了。

原本C++还有一个桶排序(bucket_rank)，利用C++ std::map的有序性，以排序因子为key，value
为一个桶(vector)，把具有相同排序因子的放到同一个桶。把所有元素插入完成后，遍历这个map即
为有序的
这种方式对于大量数据一次性排序(比如全服玩家等级排序)非常适用

不过后来由于策划要求多个排序因子(等级、战力、创角时间)，那个就很少用了，而且现在多半是小服
人数少，也不用考虑效率，干脆直接从C++去掉了

如果人数不多，直接用table.sort问题都不大。如果人数多，开个定时器，每秒排一部分人，也能
满足需求

InsertionRank 是针对数量不多，排名变化不频繁，需要根据id取排序的需求来做的。比如游戏里
常见的伤害排行榜
]] local InsertionRank = require "util.insertion_rank"

local MAX_VAL = 1024000

Test.describe("rank utils test", function()
    Test.it("insertion rank base test", function()
        local rank = InsertionRank()

        local object, upsert = rank:set_factor(1, 9)
        Test.equal(upsert, true)
        Test.equal(rank:get_index(object), 1)

        rank:set_factor(2, 999)
        rank:set_factor(3, 2)
        object, upsert = rank:add_factor(1, 1)
        Test.assert(not upsert)
        Test.equal(rank:get_index(object), 2)

        -- 排序稳定性测试，当前object应该排在前一个factor也为10的后面
        object, upsert = rank:add_factor(4, 10)
        Test.equal(upsert, true)
        Test.equal(rank:get_index(object), 3)

        -- 随机排序测试
        local max_id = 500
        local text = "这个是emoji😎🤗符号存储"
        rank:clear()
        for i = 1, max_id do
            object = rank:set_factor(i, math.random(1, MAX_VAL))
            object.text = text .. i
            object.level = math.random(1, MAX_VAL)
            object.fight = math.random(1, MAX_VAL)
        end
        for i = 1, max_id do rank:add_factor(i, math.random(1, MAX_VAL)) end

        rank:remove(1) -- 测试remove
        local max_count = rank:count()
        Test.equal(max_count, max_id - 1)

        -- 校验排行榜的正确性
        local last = MAX_VAL + MAX_VAL
        for i = 1, max_count do
            object = rank:get_object_by_index(i)
            local f = rank:get_factor(object)

            Test.assert(f <= last)
            last = f
        end

        -- 文件save、load测试
        rank:save("runtime/rank/test.json")
        local other = InsertionRank("test.json")
        other:load()
        Test.equal(other:count(), rank:count())
        for i = 1, other:count() do
            local other_obj = other:get_object_by_index(i)
            local rank_obj = rank:get_object_by_index(i)
            Test.equal(other_obj, rank_obj)
        end

        -- 上面默认倒序排列，现在测试升序
        rank:clear()
        rank:using_asc()
        for i = 1, max_id do rank:set_factor(i, math.random(1, MAX_VAL)) end
        -- 校验排行榜的正确性
        last = 0
        for i = 2, max_count + 1 do
            object = rank:get_object_by_index(i)
            local f = rank:get_factor(object)

            Test.assert(f >= last)
            last = f
        end

        -- 排序上限测试
        rank:clear()
        max_id = 10
        rank:set_max(max_id / 2)
        for i = 1, max_id do rank:set_factor(i, math.random(1, 1024000)) end
        Test.equal(rank:count(), max_id / 2)

        -- 多因子排序测试(自定义comp函数测试)
        -- 现在这种做法会出现热更更新不到comp的函数，解决办法是
        -- 1. 把真正的comp函数包在一个闭包里
        -- 2. 热更的时候重新设置comp函数
        -- TODO 后期看下能不能做一个统一的注册机制解决这个问题
        rank:clear()
        rank:set_max(10240)
        rank.comp = function(a, b)
            -- 先对比积分
            if a.f ~= b.f then return a.f > b.f and 1 or -1 end
            -- 积分一样，对比等级
            if a.level ~= b.level then
                return a.level > b.level and 1 or -1
            end
            -- 等级也一样，对比战力
            if a.fight == b.fight then return 0 end
            return a.fight > b.fight and 1 or -1
        end
        for i = 1, max_id do
            -- 多个排序因子，不要直接使用set_factor，那时level和fight没设置，不能对比
            -- rank:set_factor(i, math.random(1, MAX_VAL))
            object = rank:new_object(i, math.random(1, MAX_VAL))
            object.level = math.random(1, 10240)
            object.fight = math.random(1, 10240000)
            rank:adjust(object)
        end
        for i = 1, max_id do
            -- 更新数据的时候，因为已经有了level和fight，
            -- 如果只更新factor，那可以用set_factor或者add_factor
            if i % 2 == 1 then
                rank:add_factor(i, math.random(1, 10))
            else
                -- 如果需要更新多个排序因子，则先设置好，再调用adjust
                object = rank:new_object(i)
                object.f = object.f + math.random(1, 10)
                object.fight = object.fight + math.random(1, 10)
                rank:adjust(object)
            end
        end
        -- 校验排行榜的正确性
        local last_obj = rank:get_object_by_index(1)
        for i = 2, rank:count() do
            local cur_obj = rank:get_object_by_index(i)
            Test.assert(rank.comp(last_obj, cur_obj) >= 0)
        end
    end)

    local MAX_PERF = 512
    local MAX_UPDATE = 100000
    Test.it(
        string.format("insertion rank %d object %d update perf test", MAX_PERF,
                      MAX_UPDATE), function()
            local rank = InsertionRank()
            for i = 1, MAX_PERF do
                rank:set_factor(i, math.random(1, MAX_VAL))
            end
            for _ = 1, MAX_UPDATE do
                rank:set_factor(math.random(1, MAX_PERF),
                                math.random(1, MAX_VAL))
            end
        end)
end)

