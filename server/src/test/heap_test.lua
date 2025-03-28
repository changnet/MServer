local MAX_PERF_TIMES = 20000

Test.describe("heap test", function()
    Test.it("max heap test", function()
        -- 最大堆
        local Heap = require "util.heap"

        local tbl = {9, 6, 7, 4, 8, 3, 2, 1, 5}
        local expect = {}
        local mh = Heap()
        for _, val in pairs(tbl) do
            mh:push(val)
            table.insert(expect, val)
        end

        table.sort(expect, function(a, b)
            return a > b
        end)

        local index = 1
        while not mh:empty() do
            local obj = mh:top()
            local exp = expect[index]

            Test.equal(obj, exp)

            local pv = mh:pop()
            Test.equal(pv, exp)
            index = index + 1
        end
    end)

    Test.it("min heap test", function()
        -- 最小堆
        local Heap = require "util.heap"

        local tbl = {9, 6, 7, 4, 8, 3, 2, 1, 5}
        local expect = {}
        local mh = Heap(function(a, b) return a < b end)
        for _, val in pairs(tbl) do
            mh:push(val)
            table.insert(expect, val)
        end

        table.sort(expect, function(a, b)
            return a < b
        end)

        local index = 1
        while not mh:empty() do
            local obj = mh:top()
            local exp = expect[index]

            Test.equal(obj, exp)

            local pv = mh:pop()
            Test.equal(pv, exp)
            index = index + 1
        end
    end)

    Test.it("heap perf test " .. MAX_PERF_TIMES, function()
        local Heap = require "util.heap"

        local mh = Heap()
        for _ = 1, MAX_PERF_TIMES do
            mh:push(math.random(1, 10000000))
        end

        while not mh:empty() do
            mh:pop()
        end
    end)

    Test.it("heap fuzzy test", function()
        local Heap = require "util.heap"

        local expect = {}
        local mh = Heap()
        for _ = 1, 20000 do
            local v = math.random(1, 10000000)
            if 0 == v % 5 and #expect > 0 then
                local rindex = math.random(1, #expect)
                local element = expect[rindex]

                local rvalue = mh:erase(element[1])
                Test.equal(rvalue, element[2])
                table.remove(expect, rindex)
            else
                local id = mh:push(v)
                table.insert(expect, {id, v})
            end
        end

        table.sort(expect, function(a, b)
            return a[2] > b[2]
        end)

        local index = 1
        while not mh:empty() do
            local obj = mh:top()
            local exp = expect[index]

            Test.equal(obj, exp[2])

            local pv = mh:pop()
            Test.equal(pv, exp[2])
            index = index + 1
        end
    end)
end)
