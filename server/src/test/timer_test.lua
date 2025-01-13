-- 定时器精度测试
-- xzc
-- 2022-01-16

local Timer = require "timer.timer"

Test.describe("timer test", function()
    Test.it("timer interval test", function()
        local after = 69
        local msec = 17
        local times = 6

        local now_ms = Engine.steady_clock()
        local next_ms = now_ms + after

        -- interval的精度应该是1毫秒
        local timer_interval_test = function()
            -- printf("timer interval expect %d, got %d", next_ms, Engine.clock())
            local val = math.abs(Engine.steady_clock() - next_ms)
            if val > 1 then
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
            if val > 1 then
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
    end)
end)
