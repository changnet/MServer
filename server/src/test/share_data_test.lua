
local g_sharedata = g_sharedata

Test.describe("share data test", function()
    Test.it("share data base test", function()
        -- 1. 基本类型的 set 和 get
        g_sharedata:set("base", "int", 100)
        Test.equal(g_sharedata:get("base", "int"), 100)

        g_sharedata:set("base", "string", "hello")
        Test.equal(g_sharedata:get("base", "string"), "hello")

        g_sharedata:set("base", "bool", true)
        Test.equal(g_sharedata:get("base", "bool"), true)

        g_sharedata:set("base", "double", 3.14)
        Test.equal(g_sharedata:get("base", "double"), 3.14)

        -- 2. 嵌套 table 的 set 和 get
        local complex = {
            a = 1,
            b = "str",
            c = { d = 4, e = { f = 5 } },
            [100] = "num_key",
            [true] = "bool_key"
        }
        g_sharedata:set("complex", complex)
        local res = g_sharedata:get("complex")
        Test.equal(res.a, 1)
        Test.equal(res.b, "str")
        Test.equal(res.c.e.f, 5)
        Test.equal(res[100], "num_key")
        Test.equal(res[true], "bool_key")

        -- 3. unset 测试
        g_sharedata:unset("base", "int")
        Test.equal(g_sharedata:get("base", "int"), nil)
        
        g_sharedata:set("to_unset", { x = 1, y = 2 })
        g_sharedata:unset("to_unset", "x")
        Test.equal(g_sharedata:get("to_unset", "x"), nil)
        Test.equal(g_sharedata:get("to_unset", "y"), 2)

        -- 4. fetch_add 测试 (原子加)
        g_sharedata:set("counter", 10)
        local old = g_sharedata:fetch_add("counter", 5)
        Test.equal(old, 10)
        Test.equal(g_sharedata:get("counter"), 15)

        -- 5. fetch_into 测试 (批量获取字段到 table)
        g_sharedata:set("user", { name = "andy", age = 18, sex = 1 })
        local cache = {}

        g_sharedata:fetch_into("user", { name = 1, age = 1 }, cache)
        
        Test.equal(cache.name, "andy")
        Test.equal(cache.age, 18)
        Test.equal(cache.sex, nil)

        -- 6. keys 测试
        g_sharedata:set("keys_test", { k1 = 1, k2 = 2, [10] = 3 })
        local ks = g_sharedata:keys("keys_test")
        Test.equal(ks.n, 3)
        -- 检查 key 是否存在（顺序不保证）
        local found = {}
        for i = 1, ks.n do found[ks[i]] = true end
        Test.equal(found.k1, true)
        Test.equal(found.k2, true)
        Test.equal(found[10], true)

        -- 7. update 测试 (合并更新)
        g_sharedata:set("update_test", { a = 1, b = 2 })
        g_sharedata:update("update_test", { b = 20, c = 30 })
        local up_res = g_sharedata:get("update_test")
        Test.equal(up_res.a, 1)
        Test.equal(up_res.b, 20)
        Test.equal(up_res.c, 30)

        -- 8. memory_size 测试
        local msize = g_sharedata:memory_size()
        Test.assert(msize > 0)

        -- 9. 多级 Key 导航测试 (类数组元素访问)
        g_sharedata:set("list", 1, {a = 1, b = "b", c = 122.8})
        Test.equal(g_sharedata:get("list", 1, "c"), 122.8)
        Test.equal(g_sharedata:get("list", 1, "a"), 1)

        -- 10. int64 测试
        -- 在 Lua 5.3+ 中，整数默认是 64 位的
        local max_int64 = 9223372036854775807 -- 2^63 - 1
        g_sharedata:set("int64_test", "max", max_int64)
        local val = g_sharedata:get("int64_test", "max")
        -- 注意：如果 C++ 底层使用 double 存储数值，此处可能会因精度丢失而失败
        Test.equal(val, max_int64)

        local min_int64 = -9223372036854775808
        g_sharedata:set("int64_test", "min", min_int64)
        Test.equal(g_sharedata:get("int64_test", "min"), min_int64)
    end)

    Test.it("share data perf test", function()
        -- 创建一个数量为10000的table进行对比
        local times = 10000

        local m1 = collectgarbage("count")
        local c1 = os.clock()
        local list = {}
        for i = 1, times do
            list[i] = {id = i, name = "name_" .. i, level = i}
        end
        local m2 = collectgarbage("count")
        local c2 = os.clock()

        local m3 = g_sharedata:memory_size()
        g_sharedata:set("perf_test_list", {})
        for i = 1, times do
            g_sharedata:set("perf_test_list",
                i, {id = i, name = "name_" .. i, level = i})
        end
        local c3 = os.clock()
        local m4 = g_sharedata:memory_size()
        Test.print(string.format(
            "%d write - lua table time: %.2f, memory: %.2f KB, sharedata time: %.2f, memory: %.2f KB",
            times, c2 - c1, m2 - m1, c3 - c2, (m4 - m3) / 1024))

        -- 读取性能对比
        c1 = os.clock()
        for i = 1, times do
            local v = list[i]
            if v.id ~= i then error("error") end
        end
        c2 = os.clock()

        for i = 1, times do
            local v = g_sharedata:get("perf_test_list", i)
            if v.id ~= i then error("error") end
        end
        c3 = os.clock()
        Test.print(string.format(
            "%d read - lua table time: %.2f, sharedata time: %.2f",
            times, c2 - c1, c3 - c2))

        -- 创建数量为100000的table进行对比
        times = 100000

        m1 = collectgarbage("count")
        c1 = os.clock()
        list = {}
        for i = 1, times do
            list[i] = {id = i, name = "name_" .. i, level = i}
        end
        m2 = collectgarbage("count")
        c2 = os.clock()

        m3 = g_sharedata:memory_size()
        g_sharedata:set("perf_test_list_large", {})
        for i = 1, times do
            g_sharedata:set("perf_test_list_large",
                i, {id = i, name = "name_" .. i, level = i})
        end
        c3 = os.clock()
        m4 = g_sharedata:memory_size()
        Test.print(string.format(
            "%d write - lua table time: %.2f, memory: %.2f KB, sharedata time: %.2f, memory: %.2f KB",
            times, c2 - c1, m2 - m1, c3 - c2, (m4 - m3) / 1024))
        
        -- 读取性能对比
        c1 = os.clock()
        for i = 1, times do
            local v = list[i]
            if v.id ~= i then error("error") end
        end
        c2 = os.clock()

        for i = 1, times do
            local v = g_sharedata:get("perf_test_list_large", i)
            if v.id ~= i then error("error") end
        end
        c3 = os.clock()
        local cache = {}
        local fetch_field = {id = 1}
        for i = 1, times do
            g_sharedata:fetch_into("perf_test_list_large", i, fetch_field, cache)
            if cache.id ~= i then error("error") end
        end
        c4 = os.clock() 
        Test.print(string.format(
            "%d read - lua table time: %.2f, sharedata time: %.2f, sharedata fetch_into time: %.2f",
            times, c2 - c1, c3 - c2, c4 - c3))
    end)
end)

