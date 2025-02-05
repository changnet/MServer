-- 客户端服务器，服务器之间链接测试

local ScSocket = require "network.sc_socket"
local CsSocket = require "network.cs_socket"

local sc_param = table.copy(ScSocket.default_param)
local cs_param = table.copy(CsSocket.default_param)

-- 把收发缓冲区设置得大一些，不然进行大包测试的时候溢出会断开连接
sc_param.send_chunk_max = 256
sc_param.recv_chunk_max = 256
cs_param.send_chunk_max = 256
cs_param.recv_chunk_max = 256



Test.describe("socket_test", function()
    local srv_conn = nil
    local clt_conn = nil
    local listen_conn = nil
    local listen_conn_ws = nil

    local local_host = "::1"
    local local_port = 2099
    if IPV4 then local_host = "127.0.0.1" end

    local msg_id_len = 2 -- 一个int16的长度
    local Buffer = require "engine.Buffer"

    local local_port_ws = 8085
    local local_port_wss = 8086

    local PERF_TIMES = 10000

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

    local codec = nil
    local base_pkt = nil -- 基准测试用的包，主要用于各种临界条件测试
    local lite_pkt = nil -- 简单的测试包，更接近于日常通信用的包
    local rep_cnt = 512 -- 512的时候，整个包达到50000多字节了
    Test.before(function()
        base_pkt = {
            d1 = -99999999999999.55555,
            d2 = 99999999999999.55555,
            -- float的精度超级差，基本在0.5，过不了t_equal
            -- 如11111111.5678就会变成11111112，在C++中也一样
            -- f1 = -111.6789,
            -- f2 = 111.1234,
            i1 = -2147483648;
            i2 = 2147483647;
            i641 = math.mininteger,
            i642 = math.maxinteger,
            b1 = true,
            -- b2 = false, -- pbc不传输默认值，加了这个会导致t_equal检测失败
            s1 = "s", -- 空字符串是默认值 ，pbc也不打包的
            s2 = string.rep("ssssssssss", rep_cnt),
            by1 = "s"; -- 空字符串是默认值 ，pbc也不打包的
            -- by2 = string.rep("ssssssssss", rep_cnt);
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

        local PbcCodec = require "engine.PbcCodec"
        codec = PbcCodec()

        codec:reset()
        for _, fpath in pairs({"../pb/system.pb"}) do
            local f = io.open(fpath, "rb") -- 必须以binary模式打开
            local buffer = f:read("a")
            f:close()

            local ok = codec:load(buffer)
            assert(ok)
        end
        codec:update()

        -- 建立一个客户端、一个服务端连接，模拟通信

        listen_conn = ScSocket()
        listen_conn.default_param = sc_param
        listen_conn:listen(local_host, local_port)
        listen_conn.on_accepted = function(self)
            srv_conn = self
        end
        listen_conn.on_connected = function() end
        listen_conn.on_disconnected = function() end

        clt_conn = CsSocket()
        clt_conn.default_param = cs_param
        clt_conn:connect(local_host, local_port)
        clt_conn.on_connected = function()
            Test.done()
        end
        clt_conn.on_disconnected = function() end

        Test.wait(2000)
    end)

    Test.it("client socket test", function()
        -- TODO 其实不用protobuf测试也行，只是这里想模拟一下整个cs通信流程的延迟
        local msg_id = math.random(1, 65535)
        srv_conn.on_message = function(self, recv_msg_id, buffer, size)
            Test.equal(recv_msg_id, msg_id)

            -- 读取一个int16
            local msg_id2, codec_buffer = Buffer.read_int(buffer, msg_id_len)
            Test.equal(msg_id2, msg_id)
            local pkt = codec:decode_from_buffer(
                "system.TestBase", codec_buffer, size - msg_id_len)
            Test.equal(pkt, base_pkt)

            self:send_pkt(msg_id, codec_buffer, size - msg_id_len)
        end
        clt_conn.on_message = function(self, recv_msg_id, buffer, size)
            Test.equal(recv_msg_id, msg_id)

            local msg_id2, codec_buffer = Buffer.read_int(buffer, msg_id_len)
            Test.equal(msg_id2, msg_id)

            local pkt = codec:decode_from_buffer(
                "system.TestBase", codec_buffer, size - msg_id_len)
            Test.equal(pkt, base_pkt)

            Test.done()
        end

        local buffer, size = codec:encode_to_buffer("system.TestBase", base_pkt)
        clt_conn:send_pkt(msg_id, buffer, size)

        Test.wait()
    end)
    --[[
    Test.it(string.format("protobuf performance test %d", PERF_TIMES), function()
        local count = 0
        Cmd.reg(TEST.LITE, function(pkt)
            srv_conn:send_pkt(TEST.LITE, pkt)
        end, true)
        clt_conn.on_message = function(self, cmd, e, pkt)
            Test.equal(cmd, TEST.LITE.i)
            count = count + 1

            if count >= PERF_TIMES then Test.done() end
        end

        -- 一次性发送大量数据，测试缓冲区及打包效率
        -- 由于大量的包堆在缓冲区，需要用到多个缓冲区块，有很多using continuous buffer日志
        for _ = 1, PERF_TIMES do
            clt_conn:send_pkt(TEST.LITE, lite_pkt)
        end

        Test.wait()
    end)
    Test.it(string.format("protobuf pingpong test %d", PERF_TIMES), function()
        local count = 0
        Cmd.reg(TEST.LITE, function(pkt)
            srv_conn:send_pkt(TEST.LITE, pkt)
        end, true)
        clt_conn.on_message = function(self, cmd, e, pkt)
            Test.equal(cmd, TEST.LITE.i)
            count = count + 1

            if count >= PERF_TIMES then
                Test.done()
            else
                clt_conn:send_pkt(TEST.LITE, lite_pkt)
            end
        end

        -- 测试来回发送数据，这个取决于打包、传输效率
        clt_conn:send_pkt(TEST.LITE, lite_pkt)

        Test.wait()
    end)
    ]]

    Test.after(function()
        if clt_conn then clt_conn:close() end
        if srv_conn then srv_conn:close() end
        if listen_conn then listen_conn:close() end
        if listen_conn_ws then listen_conn_ws:close() end
    end)
end)
