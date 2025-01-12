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
        Test.async(10000)
        local timer_interval_test = function()
            -- printf("timer interval expect %d, got %d", next_ms, ev:ms_time())
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
        Timer.interval(after, msec, times, timer_interval_test)
    end)

    Test.it("timer periodic test", function()
        local after = 3
        local sec = 2
        local times = 3

        local now = Engine.system_clock()
        local next_s = now + after

        Test.async(10000)
        local timer_periodic_test = function()
            -- printf("timer periodic expect %d, got %d", next_s, ev:time())
            local val = math.abs(Engine.system_clock() - next_s)
            if val > 1 then
                Test.print("timer periodic precision = " .. val)
                Test.assert(false)
            end

            times = times - 1
            next_s = next_s + sec
            if times <= 0 then Test.done() end
        end

        Rtti.name_func("timer_periodic_test", timer_periodic_test)
        Timer.periodic(after, sec, times, timer_periodic_test)
    end)
end)
