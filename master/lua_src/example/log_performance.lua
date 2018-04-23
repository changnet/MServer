-- log_performance.lua
-- 2016-03-09
-- xzc

local min_log = "add min log ........................................ %d"
local mid_log = "add mid log ........................................ %d ................................................... mid log"
local max_log = "add max log ........................................ %d ................................................... max log"

for idx = 1,256 do
    max_log = max_log .. "\n................................................... max log"
end

print( "max log length",string.len(max_log) )

local Log = require "Log"

g_log_mgr = Log()
g_log_mgr:start( 3,0 );

Log.mkdir_p( "log")

local max_insert = 1024
for i = 1,max_insert do
    g_log_mgr:write( "log/min.log",string.format( min_log,i) )
    --g_log_mgr:write( "log/mid.log",string.format( mid_log,i) )
    --g_log_mgr:write( "log/max.log",string.format( max_log,i) )
end
