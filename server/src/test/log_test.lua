-- log_performance.lua
-- 2016-03-09
-- xzc
local Log = require "engine.Log"
local util = require "engine.util"
local time = require "global.time"

t_describe("log test", function()
    t_it("log base test", function()
        local max_insert = 1024
        local log_fmt =
            "lllllllllllllllllllllllllllllllllllllllllllllllllllllllll" ..
                "lllllllllllllllllllllllllllllllllllllllllllllllllllllllll" ..
                "lllllllllllllllllllllllllllllllllllllllllllllllllllllllll" ..
                "log 日志 %d!"

        -- 如果不存在，则创建日志目录
        util.mkdir_p("log")

        -- 创建独立的线程
        local logger = Log("test_logger")
        logger:start(3000) -- 3000毫秒写入一次

        -- 测试daily
        -- 按天滚动日志不太好测试，不过由于runtime是基于这个的，应该也不需要怎么测试
        -- 1. 测试时调时间， 弄一个接口修改文件日期为昨天
        -- touch ../server/bin/log/test_runtime -t 202103101030 修改文件时间
        local yesterday = ev:time() - 86400
        local tm = time.ctime(yesterday)
        local y_file = string.format("log/test_log_daily%04d%02d%02d", tm.year,
                                     tm.month, tm.day)
        os.remove(y_file)
        os.remove("log/test_log_daily")
        logger:set_option("log/test_log_daily", Log.PT_DAILY)

        local half = max_insert / 2

        -- 先写入昨天的日志
        for i = 1, half do
            logger:append_log_file("log/test_log_daily",
                                   string.format(log_fmt, i), yesterday)
        end
        -- 写入今天的日志，昨天的将被移到另一个文件
        for i = half, max_insert do
            logger:append_log_file("log/test_log_daily",
                                   string.format(log_fmt, i))
        end

        -- plog和elog会在屏幕打印数据，就不测试了。平常看runtime就可以了
        -- logger:plog("plog test ----------")
        -- logger:eprint("eprint test ----------")

        -- 测试size及其滚动、测试logfile
        os.remove("log/test_log_size")
        for i = 1, 10 do
            os.remove(string.format("log/test_log_size.%03d", i))
        end
        logger:set_option("log/test_log_size", Log.PT_SIZE, 20480)
        for i = 1, max_insert do
            logger:append_log_file("log/test_log_size",
                                   string.format(log_fmt, i))
        end

        -- 测试file，这个可以不调用set_option
        os.remove("log/test_file")
        for i = 1, max_insert do
            logger:append_file("log/test_file", string.format(log_fmt, i))
        end

        logger:stop() -- 这里会阻塞到文件写入完成

        -- 校验刚刚写入的文件
        local fyd = io.open(y_file, "rb")
        t_assert(fyd)
        local ctx_fyd = fyd:read("a")
        t_assert(string.find(ctx_fyd, string.format(log_fmt, 1)))
        t_assert(string.find(ctx_fyd, string.format(log_fmt, half)))
        fyd:close()
        local fd = io.open("log/test_log_daily", "rb")
        t_assert(fd)
        local ctx_fd = fd:read("a")
        t_assert(string.find(ctx_fd, string.format(log_fmt, half + 1)))
        t_assert(string.find(ctx_fd, string.format(log_fmt, max_insert)))
        fd:close()

        local f0 = io.open("log/test_log_size", "rb")
        t_assert(f0)
        local ctx0 = f0:read("a")
        t_assert(string.find(ctx0, string.format(log_fmt, max_insert)))
        f0:close()

        local f10 = io.open("log/test_log_size.010", "rb")
        t_assert(f10)
        local ctx10 = f10:read("a")
        t_assert(string.find(ctx10, string.format(log_fmt, 1)))
        f10:close()

        local ff = io.open("log/test_file", "rb")
        t_assert(ff)
        local ctx_ff = ff:read("a")
        t_assert(string.find(ctx_ff, string.format(log_fmt, 1)))
        t_assert(string.find(ctx_ff, string.format(log_fmt, max_insert)))
        ff:close()
    end)
end)
