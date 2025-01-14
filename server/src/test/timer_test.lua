-- 定时器精度测试
-- xzc
-- 2022-01-16

local Timer = require "timer.timer"

Test.describe("timer test", function()
    -- 精度应该是1毫秒
    -- 但实际上底层thread_condition的wait的最小时间就是1毫秒
    -- 还要算上底层回调到lua，以及在lua中运行到检测时间所耗时间，因此精度为1会偶尔测试不过
    -- 即使是2，如果在测试的函数中运行耗时比较大的函数（比如for循环执行，直接输出日志到stdio）也可能会导致测试不通过
    local accuracy = 2
    Test.it("timer interval test", function()
        local after = 69
        local msec = 17
        local times = 6

        local now_ms = Engine.steady_clock()
        local next_ms = now_ms + after


        local timer_interval_test = function()
            -- printf("timer interval expect %d, got %d", next_ms, Engine.clock())
            local val = math.abs(Engine.steady_clock() - next_ms)
            if val > accuracy then
                Test.print("timer interval precision  lost", val)
                Test.assert(false)
            end

            times = times - 1
            next_ms = next_ms + msec
            if times <= 0 then Test.done() end
        end

        Rtti.name_func("timer_interval_test", timer_interval_test)

        -- 保证帧时间和实时时间一致，不然定时器就会有误差
        Test.assert(now_ms, Engine.clock())
        Timer.interval(after, msec, times, timer_interval_test)

        Test.wait(10000)
        Timer.clear() -- 清除所有定时器，避免出错或者超时后定时器回调影响其他测试
    end)

    Test.it("timer periodic test", function()
        local after = 3
        local sec = 2
        local times = 3

        local now = Engine.system_clock()
        local next_s = now + after

        local timer_periodic_test = function()
            -- printf("timer periodic expect %d, got %d", next_s, ev:time())
            local val = math.abs(Engine.system_clock() - next_s)
            if val > accuracy then
                Test.print("timer periodic precision lost", val)
                Test.assert(false)
            end

            times = times - 1
            next_s = next_s + sec
            if times <= 0 then Test.done() end
        end

        Rtti.name_func("timer_periodic_test", timer_periodic_test)

        -- 保证帧时间和实时时间一致
        Test.assert(now, Engine.time_ms())
        Timer.periodic(after, sec, times, timer_periodic_test)

        Test.wait(10000)
        Timer.clear() -- 清除所有定时器，避免出错或者超时后定时器回调影响其他测试
    end)

    Test.it("timer wait test", function()
        -- wait只能在CoPool中使用
        CoPool.invoke(function()
            for _, ms in pairs({1, 300, 5, 20, 88, 0}) do
                local next_ms = Engine.clock() + ms
                Timer.wait(ms)

                local clock = Engine.clock()
                local diff = math.abs(clock - next_ms)
                if diff > accuracy then
                    Test.assert(next_ms, clock)
                end
            end
            Test.done()
        end)

        Test.wait(10000)
        Timer.clear() -- 清除所有定时器，避免出错或者超时后定时器回调影响其他测试
    end)

    Test.it("timer fuzzy test", function()
        -- 不断地添加、删除定时器，主要是为了测试底层定时器结构维护的正确性
        -- wait只能在CoPool中使用
        CoPool.invoke(function()
            local list = {}
            local beg_clock = Engine.clock()
            for i = 1, 1000 do
                local r = math.random(10000) % 2
                if 0 == r then
                    local after = math.random(1, 2000)
                    local next_ms = beg_clock + after
                    local id = Timer.timeout(after, function()
                        local clock = Engine.clock()
                        local diff = math.abs(clock - next_ms)
                        if diff > accuracy then
                            Test.assert(next_ms, clock)
                        end
                        local found = false
                        for idx, t in pairs(list) do
                            if t.i == i then
                                found = true
                                table.remove(list, idx)
                                break
                            end
                        end
                        Test.assert(found, "timer not found")
                    end)
                    table.insert(list, {i = i, id = id, after = after})
                elseif 1 == r then
                    if #list > 0 then
                        local idx = math.random(#list)
                        local t = list[idx]
                        Timer.stop(t.id)
                        table.remove(list, idx)
                    end
                end

                -- 等待一部分定时器超时
                if 0 == i % 250  then Timer.wait(500) end
            end
            local after = 0
            for _, t in pairs(list) do
                if t.after > after then after = t.after end
            end

            Timer.wait(after) -- 等待所有定时器超时
            Test.equal(#list, 0)
            Test.done()
        end)

        Test.wait(10000)
        Timer.clear() -- 清除所有定时器，避免出错或者超时后定时器回调影响其他测试
    end)
end)
