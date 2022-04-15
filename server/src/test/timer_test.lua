-- 定时器精度测试
-- xzc
-- 2022-01-16

local Timer = require "timer.timer"

t_describe("timer test", function()
    t_it("timer interval test", function()
        local after = 69
        local msec = 17
        local times = 6

        local now_ms = ev:ms_time()
        local next_ms = now_ms + after

        -- interval的精度应该是1毫秒
        t_async(10000)
        local timer_interval_test = function()
            -- printf("timer interval expect %d, got %d", next_ms, ev:ms_time())
            local val = math.abs(ev:ms_time() - next_ms)
            if val > 1 then
                t_print("timer interval precision = " .. val)
                t_assert(false)
            end

            times = times - 1
            next_ms = next_ms + msec
            if times <= 0 then t_done() end
        end

        name_func("timer_interval_test", timer_interval_test)
        Timer.interval(after, msec, times, timer_interval_test)
    end)

    -- periodic的精度是1秒
    t_it("timer periodic test", function()
        local after = 3
        local sec = 2
        local times = 3

        local now = ev:time()
        local next_s = now + after

        t_async(10000)
        local timer_periodic_test = function()
            -- printf("timer periodic expect %d, got %d", next_s, ev:time())
            local val = math.abs(ev:time() - next_s)
            if val > 1 then
                t_print("timer periodic precision = " .. val)
                t_assert(false)
            end

            times = times - 1
            next_s = next_s + sec
            if times <= 0 then t_done() end
        end

        name_func("timer_periodic_test", timer_periodic_test)
        Timer.periodic(after, sec, times, timer_periodic_test)
    end)
end)
