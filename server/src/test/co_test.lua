-- 协程测试
-- xzc
-- 2025-01-19

Test.describe("coroutine test", function()
    Test.it("coroutine test", function()
        -- 保证传参数及返回的正确性

        local params = {1, 2, 3, 123.789, true, false, "foo", nil, {1, 2, 3}}

        local func = function(...)
            local p = {...}
            Test.equal(p, params)
            local p2 = {coroutine.yield(CoPool.current_session())}

            Test.equal(p2, params)
            return params
        end

        local ok, session = CoPool.invoke(func, table.unpack(params))
        Test.assert(ok)

        local rok = CoPool.resume(session, table.unpack(params))
        Test.assert(rok)

        -- 重新测试，保证co复用时正确性
        for _ = 1, 999999 do
            local idle_count = CoPool.idle_count()
            ok, session = CoPool.invoke(func, table.unpack(params))
            Test.assert(ok)

            -- 保证有使用池
            Test.equal(CoPool.idle_count(), idle_count - 1)
            rok = CoPool.resume(session, table.unpack(params))
            Test.assert(rok)
        end
    end)

    Test.it("coroutine perf test", function()
        local func = function()
            coroutine.yield(CoPool.current_session())
        end
        local func2 = function() end

        --[[
        [T1LP01-20 20:23:29]    1000 coroutine memory usage 1220.19 kb
        [T1LP01-20 20:23:29]    1000 rtti callback memory usage 78.12 kb
        [T1LP01-20 20:23:29]    1000 coroutine resume time 3
        [T1LP01-20 20:23:29]    1000 rtti callback time 0
        ]]

        local count = 1000

        -- 创建1000个协程消耗多少内存
        local mem = collectgarbage("count")
        for _ = 1, count do
            CoPool.invoke(func)
        end
        print(string.format("%d coroutine memory usage %.2f kb",
             count, collectgarbage("count") - mem))

        -- 创建1000个普通回调消耗多少内存
        mem = collectgarbage("count")
        for _ = 1, count do
            Rtti.make_func_cb(func)
        end
        print(string.format("%0.f rtti callback memory usage %.2f kb",
             count, collectgarbage("count") - mem))

        -- 唤醒1000协程需要多少时间
        local ss_list = {}
        for _ = 1, count do
            local ok, ss = CoPool.invoke(func)
            Test.assert(ok)
            table.insert(ss_list, ss)
        end
        local b1 = Test.clock()
        for _, session in pairs(ss_list) do
            CoPool.resume(session)
        end
        local e1 = Test.clock()
        print(string.format("%0.f coroutine resume time %d ", count, e1 - b1))

        local cb_list = {}
        for _ = 1, count do
            table.insert(cb_list, Rtti.make_func_cb(func2))
        end
        local b2 = Test.clock()
        for _, cb in pairs(cb_list) do
            cb()
        end
        local e2 = Test.clock()
        print(string.format("%0.f rtti callback time %d ", count, e2 - b2))
    end)

end)
