-- statistic.lua
-- xzc
-- 2019-04-06

-- 统计服务器运行情况

local Statistic = oo.singleton( nil,... )

function Statistic.collect()
    local total_stat = {}
    -- "count": returns the total memory in use by Lua (in Kbytes)
    total_stat.lua_mem = collectgarbage("count")

    vd( obj_counter.dump() )

    total_stat.lua_obj = oo.stat()
    vd(total_stat)

    return total_stat
end

local stat = Statistic()
return stat
