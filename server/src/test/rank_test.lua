-- 排行榜算法测试

-- local BucketRank = require "util.bucket_rank"
local InsertionRank = require "util.insertion_rank"

t_describe("rank utils test", function()
    t_it("insertion rank base test", function()
        local rank = InsertionRank()

        local object, upsert = rank:set_factor(1, 9)
        t_equal(upsert, true)
        t_equal(1 == rank:get_index(object))

        rank:set_factor(2, 999)
        rank:set_factor(3, 2)
        object, upsert = rank:add_factor(1, 1)
        t_assert(not upsert)
        t_equal(rank:get_index(object), 2)

        -- 排序稳定性测试
        object, upsert = rank:add_factor(4, 10)
        t_equal(upsert, true)
        t_equal(3 == rank:get_index(object))

        -- 随机排序测试
        local max_id = 500
        rank:clear()
        for i = 1, max_id do
            rank:set_factor(i, math.random(1, 10240000))
        end
        for i = 1, max_id do
            rank:add_factor(i, math.random(1, 10240000))
        end

        rank:remove(1) -- 测试remove
        local max_count = rank:get_count()
        t_equal(max_count, max_id - 1)

        -- 校验排行榜的正确性
        local last = 0
        for i = 2, max_count + 1 do
            object = rank:get_object_by_index(i)
            local f = rank:get_factor(object)

            t_assert(f > last)
            last = f
        end

        -- 排序上限测试
        rank:clear()

        max_id = 10
        rank:set_max(max_id / 2)
        for i = 1, max_id do
            rank:set_factor(i, math.random(1, 1024000))
        end
        t_equal(rank:get_count(), max_id / 2)

        -- 多因子排序测试
        -- 文件save、load测试
        -- 默认的升序、倒序测试
        -- 自定义comp函数测试
        -- 自定义comp函数热更问题
    end)

    t_it("insertion rank perf test", function()
    end)
end)

