-- log_performance.lua
-- 2016-03-09
-- xzc

local util = require "util"

t_describe("log test", function()
    local logger = nil
    local max_insert = 1024
    local log_fmt = "lllllllllllllllllllllllllllllllllllllllllllllllllllllllll"
        .. "lllllllllllllllllllllllllllllllllllllllllllllllllllllllll"
        .. "lllllllllllllllllllllllllllllllllllllllllllllllllllllllll"
        .. "log 日志 %d"
    t_before(function()
        -- 如果不存在，则创建日志目录
        util.mkdir_p( "log")

        -- 删除旧的测试日志
        util.file_rm("log/test_log_size")
        util.file_rm("log/test_log_size.1")
        util.file_rm("log/test_log_size.2")
        util.file_rm("log/test_log_size.3")
        util.file_rm("log/test_log_size.3")
        util.file_rm("log/test_log_daily")

        local tm = time.ctime()
        util.file_rm("log/test_log_daily")
        util.file_rm(string.format(
            "log/test_log_daily%d%02d%02d", tm.year, tm.month, tm.day))

        -- 创建独立的线程
        logger = Log()
        logger:start( 3,0 )
    end)
    t_it("log base test", function()
        -- 测试daily
        -- 按天滚动日志不太好测试，不过由于runtime是基于这个的，应该也不需要怎么测试
        -- 1. 测试时调时间， 弄一个接口修改文件日期为昨天

        -- 测试size及其滚动
        for i = 1,max_insert do
            logger:write(
                "log/test_log_size%SIZE20480%", string.format(log_fmt,i))
        end
        -- 测试logfile
        -- 测试file
    end)

    t_after(function()
        logger:stop()
    end)
end)
