-- statistic.lua
-- xzc
-- 2019-04-06

-- 统计服务器运行情况

local statistic = require "statistic"
local Statistic = oo.singleton( ... )

function Statistic.collect()
    local total_stat = {}
    total_stat.app = g_app.srvname
    -- "count": returns the total memory in use by Lua (in Kbytes)
    total_stat.lua_mem = collectgarbage("count")

    total_stat.lua_obj = oo.stat()

    local cpp_stat = statistic.dump()
    local pkt_stat,rpc_stat = statistic.dump_pkt()

    table.merge(cpp_stat,total_stat)
    table.merge(pkt_stat,total_stat)
    table.merge(rpc_stat,total_stat)
    -- vd(total_stat)
    vd(pkt_stat)
    vd(rpc_stat)

    -- rpc调用暂时不加到这里，通过 @rpc_perf 1 指令查看

    return total_stat
end

local stat = Statistic()

local function rpc_stat()
    return stat:collect()
end

g_rpc:declare("rpc_stat",rpc_stat,-1)

return stat
