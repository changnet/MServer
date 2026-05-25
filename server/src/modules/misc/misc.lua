-- misc.lua
-- 2018-02-06
-- xzc

-- 一些杂七杂八的小接口
Misc = {}

-- 重载脚本
function Misc.reload()
    local t1 = Engine.steady_clock()
    local m1 = collectgarbage("count")

    __unrequire()
    Startup.load(true)

    local m2 = collectgarbage("count")
    local t = Engine.steady_clock() - t1

    local info = {
            t = t, m1 = m1, m2 = m2, name = LOCAL_NAME,
    }
    if LOCAL_ADDR ~= MAIN_ADDR then
        return info
    end

    local wait_size = 0
    local info_tbl = {info}
    local reload_finish = function(ret_info)
        wait_size = wait_size - 1
        if wait_size > 0 then
            table.insert(info_tbl, ret_info)
            return
        end

        table.sort(info_tbl, function(a, b) return a.name < b.name end)

        local str_tbl = {}
        for _, d in pairs(info_tbl) do
            local str = string.format(
                "    name = %s, time = %d, m1 = %.0f, m2 = %.0f",
                d.name, d.t, d.m1, d.m2
            )
            table.insert(str_tbl, str)
        end

        local t2 = Engine.steady_clock() - t1
        printf("script reloaded, time %d ms\n%s",
            t2, table.concat(str_tbl, "\n"))
    end
    for addr in pairs(WorkerData) do
        wait_size = wait_size + 1
        Callback[addr].Misc.reload(reload_finish)
    end
end

return Misc
