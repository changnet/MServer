
local g_sharedata = g_sharedata

Test.describe("share data test", function()
    Test.it("share data base test", function()
        local msize = g_sharedata:memory_size()
        Test.equal(msize, 16)
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
        g_sharedata.set("perf_test_list", {})
        for i = 1, times do
            g_sharedata.set("perf_test_list",
                i, {id = i, name = "name_" .. i, level = i})
        end
        local c3 = os.clock()
        local m4 = g_sharedata:memory_size()
        Test.print(string.format(
            "%d lua table time: %.2f, memory: %.2f, sharedata time: %.2f, memory: %.2f",
            times, c2 - c1, m2 - m1, c3 - c2, m4 - m3))

        if list[0] then print(list[0]) end -- remove luacheck unused warning
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
        g_sharedata.set("perf_test_list", {})
        for i = 1, times do
            g_sharedata.set("perf_test_list",
                i, {id = i, name = "name_" .. i, level = i})
        end
        c3 = os.clock()
        m4 = g_sharedata:memory_size()
        Test.print(string.format(
            "%d lua table time: %.2f, memory: %.2f, sharedata time: %.2f, memory: %.2f",
            times, c2 - c1, m2 - m1, c3 - c2, m4 - m3))
    end)
end)

