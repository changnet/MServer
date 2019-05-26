-- statistic.lua
-- xzc
-- 2019-04-06

-- 统计服务器运行情况

require "statistic"
local Statistic = oo.singleton( ... )

function Statistic.collect()
    local total_stat = {}
    -- "count": returns the total memory in use by Lua (in Kbytes)
    total_stat.lua_mem = collectgarbage("count")

    total_stat.lua_obj = oo.stat()

    local cpp_stat = statistic.dump();

    table.merge(cpp_stat,total_stat)
    vd(total_stat)

    -- rpc调用暂时不加到这里，通过 @rpc_perf 1 指令查看

    return total_stat
end

local stat = Statistic()
return stat
