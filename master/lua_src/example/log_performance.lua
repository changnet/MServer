-- log_performance.lua
-- 2016-03-09
-- xzc

local Log = require "Log"

g_log_mgr = Log()
g_log_mgr:start();

Log.mkdir_p( "test_log/money/")
Log.mkdir_p( "test_log/gold")
g_log_mgr.mkdir_p( "test_log/cmd")

local max_insert = 1
for i = 1,max_insert do
    g_log_mgr:write( "money.log",string.format( "test log %d.......................ddddddddddddddddddddddddddddddddddddddddddddddddddddddd.",i) )
    g_log_mgr:write( "gold.log",string.format( "test log %d.......................ddddddddddddddddddddddddddddddddddddddddddddddddddddddd.",i) )
end
