-- log_performance.lua
-- 2016-03-09
-- xzc

local Log  = require "Log"
local util = require "util"

t_describe("log test", function()
    t_it("log base test", function()
        local max_insert = 1024
        local log_fmt =
               "lllllllllllllllllllllllllllllllllllllllllllllllllllllllll"
            .. "lllllllllllllllllllllllllllllllllllllllllllllllllllllllll"
            .. "lllllllllllllllllllllllllllllllllllllllllllllllllllllllll"
            .. "log 日志 %d!"

        -- 如果不存在，则创建日志目录
        util.mkdir_p( "log")

        -- 创建独立的线程
        local logger = Log()
        logger:start(3000000) -- 3000000微秒写入一次

        -- 测试daily
        -- 按天滚动日志不太好测试，不过由于runtime是基于这个的，应该也不需要怎么测试
        -- 1. 测试时调时间， 弄一个接口修改文件日期为昨天
        local tm = time.ctime()
        os.remove("log/test_log_daily")
        os.remove(string.format(
            "log/test_log_daily%d%02d%02d", tm.year, tm.month, tm.day))

        local half = max_insert / 2
        local yesterday = ev:time() - 86400
        for i = 1,half do
            logger:append_log_file(
                "log/test_log_daily%DAILY%", string.format(log_fmt,i), yesterday)
        end
        for i = half,max_insert do
            logger:append_log_file(
                "log/test_log_daily%DAILY%", string.format(log_fmt,i))
        end

        -- plog和elog会在屏幕打印数据，就不测试了。平常看runtime就可以了
        -- logger:plog("plog test ----------")
        -- logger:elog("elog test ----------")


        -- 测试size及其滚动、测试logfile
        os.remove("log/test_log_size")
        for i = 1, 10 do
            os.remove("log/test_log_size." .. i)
        end
        for i = 1,max_insert do
            logger:append_log_file(
                "log/test_log_size%SIZE20480%", string.format(log_fmt,i))
        end

        -- 测试file
        os.remove("log/test_file")
        for i = 1,max_insert do
            logger:append_file("log/test_file", string.format(log_fmt,i))
        end

        logger:stop() -- 这里会阻塞到文件写入完成

        -- 校验刚刚写入的文件
        local fd = io.open("log/test_log_daily", "rb")
        t_assert(fd)
        local ctx_fd = fd:read("a")
        t_assert(string.find(ctx_fd, string.format(log_fmt, 1)))
        t_assert(string.find(ctx_fd, string.format(log_fmt, max_insert)))
        fd:close()

        local f0 = io.open("log/test_log_size", "rb")
        t_assert(f0)
        local ctx0 = f0:read("a")
        t_assert(string.find(ctx0, string.format(log_fmt,max_insert)))
        f0:close()

        local f10 = io.open("log/test_log_size.10", "rb")
        t_assert(f10)
        local ctx10 = f10:read("a")
        t_assert(string.find(ctx10, string.format(log_fmt,1)))
        f10:close()

        local ff = io.open("log/test_file", "rb")
        t_assert(ff)
        local ctx_ff = ff:read("a")
        t_assert(string.find(ctx_ff, string.format(log_fmt, 1)))
        t_assert(string.find(ctx_ff, string.format(log_fmt, max_insert)))
        ff:close()
    end)
end)
