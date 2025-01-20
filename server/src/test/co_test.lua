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

        local rok, ret = CoPool.resume(session, table.unpack(params))
        Test.assert(rok)
        Test.equal(ret, params)

        -- 重新测试，保证co复用时正确性(lua递归循环需要将近100万层才会溢出)
        -- 所以这里多循环一些以测试堆栈是否地溢出
        for _ = 1, 999999 do
            local idle_count = CoPool.idle_count()
            ok, session = CoPool.invoke(func, table.unpack(params))
            Test.assert(ok)

            -- 保证有使用池
            Test.equal(CoPool.idle_count(), idle_count - 1)
            rok, ret = CoPool.resume(session, table.unpack(params))
            Test.assert(rok)
            Test.equal(ret, params)
        end
    end)

    Test.it("coroutine perf test", function()
        -- 创建1000个协程消耗多少内存
        -- 创建1000个普通回调消耗多少内存
        -- 唤醒1000协程需要多少时间
    end)

end)
