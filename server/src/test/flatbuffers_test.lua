-- flatbuffers协议测试

g_stat_mgr = require "statistic.statistic_mgr"

require "network.cmd"
local ScConn = require "network.sc_conn"
local CsConn = require "network.cs_conn"

local sc_param = table.copy(ScConn.default_param)
local cs_param = table.copy(CsConn.default_param)

sc_param.cdt = network_mgr.CDT_FLATBUF
cs_param.cdt = network_mgr.CDT_FLATBUF

local srv_conn = nil
local clt_conn = nil
local listen_conn = nil

t_describe("flatbuffers test", function()
    local local_host = "::1"
    local local_port = 2100
    if IPV4 then local_host = "127.0.0.1" end

    local PERF_TIMES = 1000

    local TEST = {
        -- 测试用的包
        BASE = {
            s = "system.TestBase", c = "system.TestBase", i = 1
        },
        -- 测试用的包
        LITE = {
            s = "system.TestLite", c = "system.TestLite", i = 2
        },
    }

    -- https://stackoverflow.com/questions/63821960/lua-odd-min-integer-number
    -- -9223372036854775808在lua中会被解析为一个number而不是整型
    -- 使用 -9223372036854775808|0 或者 math.mininteger

    local base_pkt = nil -- 基准测试用的包，主要用于各种临界条件测试
    local lite_pkt = nil -- 简单的测试包，更接近于日常通信用的包
    local rep_cnt = 512 -- 512的时候，整个包达到50000多字节了
    t_before(function()

        -- !!! flatbuffers未发送的字段都会补上默认值，这点和pbc相反
        -- !!! 因此这里需要补齐，不然t_equal检测不会通过

        base_pkt = {
            d1 = -99999999999999.55555,
            d2 = 99999999999999.55555,
            -- float的精度超级差，基本在0.5，过不了t_equal
            -- 如11111111.5678就会变成11111112，在C++中也一样
            f1 = 0,
            f2 = 0,
            i1 = -2147483648;
            i2 = 2147483647;
            i641 = math.mininteger,
            i642 = math.maxinteger,
            b1 = true,
            b2 = false,
            s1 = "s", -- 空字符串是默认值 ，pbc也不打包的
            s2 = string.rep("ssssssssss", rep_cnt),
            by1 = "s"; -- 空字符串是默认值 ，pbc也不打包的
            by2 = string.rep("ssssssssss", rep_cnt);
            ui1 = 1,
            ui2 = 4294967295,
            ui641 = 1,
            ui642 = math.maxinteger, -- 0xffffffffffffffff, lua不支持uint64_t
            -- fix相当于 uint32, sint才是有符号的
            f321 = 2147483648,
            f322 = 2147483647,
            f641 = 1,
            f642 = math.maxinteger, -- 0xffffffffffffffff, lua不支持uint64_t
        }
        local cpy = table.copy(base_pkt)
        base_pkt.index = 0
        base_pkt.msg1 = cpy
        base_pkt.i_list = {1,2,3,4,5,99999,55555,111111111}
        base_pkt.msg_list = { cpy, cpy, cpy}

        lite_pkt = {
            b1 = true,
            i1 = math.maxinteger,
            s = "sssssssssssssssssssssssssssssssssssssssssssssssssss",
            i2 = math.mininteger,
            d1 = 99999999999999.55555,
            i3 = 2147483647,

            msg = {
                b1 = true,
                i1 = math.maxinteger,
                s = "sssssssssssssssssssssssssssssssssssssssssssssssssss",
                i2 = math.mininteger,
                d1 = 99999999999999.55555,
                i3 = 2147483647,
            }
        }

        -- 手动构建测试用的协议，参考auto_cs.lua
        Cmd.CS = { TEST = TEST }
        -- 加载协议文件
        Cmd.SCHEMA_TYPE = network_mgr.CDT_FLATBUF
        local ok = Cmd.load_flatbuffers()
        t_equal(ok, true)

        -- 建立一个客户端、一个服务端连接，模拟通信

        listen_conn = ScConn()
        listen_conn.default_param = sc_param
        listen_conn:listen(local_host, local_port)
        listen_conn.on_accepted = function(self)
            srv_conn = self
        end
        listen_conn.on_connected = function()
        end

        clt_conn = CsConn()
        clt_conn.default_param = cs_param
        clt_conn:connect(local_host, local_port)
        clt_conn.on_connected = function()
            t_done()
        end

        t_wait(2000)
    end)
    t_it("flatbuffers base", function()
        Cmd.reg(TEST.BASE, function(pkt)
            t_equal(pkt, base_pkt)
            srv_conn:send_pkt(TEST.BASE, pkt)
        end, true)
        clt_conn.on_cmd = function(self, cmd, e, pkt)
            t_equal(cmd, TEST.BASE.i)
            t_equal(pkt, base_pkt)
            t_done()
        end

        clt_conn:send_pkt(TEST.BASE, base_pkt)

        t_wait()
    end)
    t_it(string.format("flatbuffers perf test %d", PERF_TIMES), function()
        local count = 0
        Cmd.reg(TEST.LITE, function(pkt)
            srv_conn:send_pkt(TEST.LITE, pkt)
        end, true)
        clt_conn.on_cmd = function(self, cmd, e, pkt)
            t_equal(cmd, TEST.LITE.i)
            count = count + 1

            if count >= PERF_TIMES then t_done() end
        end

        -- 一次性发送大量数据，测试缓冲区及打包效率
        -- 由于大量的包堆在缓冲区，需要用到多个缓冲区块，有很多using continuous buffer日志
        for _ = 1, PERF_TIMES do
            clt_conn:send_pkt(TEST.LITE, lite_pkt)
        end

        t_wait()
    end)
    t_it(string.format("flatbuffers pingpong test %d", PERF_TIMES), function()
        local count = 0
        Cmd.reg(TEST.LITE, function(pkt)
            srv_conn:send_pkt(TEST.LITE, pkt)
        end, true)
        clt_conn.on_cmd = function(self, cmd, e, pkt)
            t_equal(cmd, TEST.LITE.i)
            count = count + 1

            if count >= PERF_TIMES then
                t_done()
            else
                clt_conn:send_pkt(TEST.LITE, lite_pkt)
            end
        end

        -- 测试来回发送数据，这个取决于打包、传输效率
        clt_conn:send_pkt(TEST.LITE, lite_pkt)

        t_wait()
    end)

    t_after(function()
        clt_conn:close()
        srv_conn:close()
        listen_conn:close()
    end)
end)
