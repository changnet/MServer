-- 客户端服务器，服务器之间链接测试

local ScSocket = require "network.sc_socket"
local CsSocket = require "network.cs_socket"
local SsSocket = require "network.ss_socket"

local sc_param = table.copy(ScSocket.default_param)
local cs_param = table.copy(CsSocket.default_param)

-- 把收发缓冲区设置得大一些，不然进行大包测试的时候溢出会断开连接
sc_param.send_byte_max = 64 * 1024 * 1024
sc_param.recv_byte_max = 64 * 1024 * 1024
cs_param.send_byte_max = 64 * 1024 * 1024
cs_param.recv_byte_max = 64 * 1024 * 1024

Test.describe("socket test", function()
    local sc_srv = nil
    local sc_clt = nil
    local sc_listen = nil
    local ss_srv = nil
    local ss_clt = nil
    local ss_listen = nil

    local ss_port = 2098
    local sc_port = 2099
    local local_host = IPV4 and "127.0.0.1" or "::1"

    local Buffer = require "engine.Buffer"

    local PERF_TIMES = 10000

    local pingpong_str = string.rep("1234567890", 512)
    local pingpong_size = string.len(pingpong_str)

    local pingpong_b = Buffer()
    local pingpong_ud = pingpong_b:fromstring(pingpong_str)
    _G.pingpong_b = pingpong_b -- 避免被gc掉，不引用的话即使停掉gc也无法阻止它被gc掉

    -- https://stackoverflow.com/questions/63821960/lua-odd-min-integer-number
    -- -9223372036854775808在lua中会被解析为一个number而不是整型
    -- 使用 -9223372036854775808|0 或者 math.mininteger

    local codec = nil
    local base_pkt = nil -- 基准测试用的包，主要用于各种临界条件测试
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

        print("pingpong buffer size is", pingpong_size)

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
        local done = 0

        sc_listen = ScSocket()
        sc_listen.default_param = sc_param
        sc_listen:listen(local_host, ss_port)
        sc_listen.on_accepted = function(self)
            sc_srv = assert(self)
        end
        sc_listen.on_connected = function() end
        sc_listen.on_disconnected = function() end

        sc_clt = CsSocket()
        sc_clt.default_param = cs_param
        sc_clt:connect(local_host, ss_port)
        sc_clt.on_connected = function()
            done = done + 1
            if 2 == done then Test.done() end
        end
        sc_clt.on_disconnected = function() end

        ss_listen = SsSocket()
        ss_listen:listen(local_host, sc_port)
        ss_listen.on_accepted = function(self)
            ss_srv = self
        end
        ss_listen.on_connected = function() end
        ss_listen.on_disconnected = function() end

        ss_clt = SsSocket()
        ss_clt:connect(local_host, sc_port)
        ss_clt.on_connected = function()
            done = done + 1
            if 2 == done then Test.done() end
        end
        ss_clt.on_disconnected = function() end

        Test.wait(2000)
    end)

    Test.it("client socket test", function()
        -- TODO 其实不用protobuf测试也行，只是这里想模拟一下整个cs通信流程的延迟
        local msg_id = math.random(1, 65535)
        sc_srv.on_message = function(self, recv_msg_id, buffer, size)
            Test.equal(recv_msg_id, msg_id)

            local pkt = codec:decode_from_buffer("system.TestBase", buffer, size)
            Test.equal(pkt, base_pkt)

            self:send_pkt(msg_id, buffer, size)
        end
        sc_clt.on_message = function(self, recv_msg_id, buffer, size)
            Test.equal(recv_msg_id, msg_id)

            local pkt = codec:decode_from_buffer("system.TestBase", buffer, size)
            Test.equal(pkt, base_pkt)

            Test.done()
        end

        local buffer, size = codec:encode_to_buffer("system.TestBase", base_pkt)
        sc_clt:send_pkt(msg_id, buffer, size)

        Test.wait()
    end)

    Test.it(string.format("client socket pingpong test %d", PERF_TIMES), function()
        -- client用的websocket，比下面的server socket耗时多一倍左右
        -- websocket 700ms，普通tcp则是400ms
        local count = 0
        local msg_id = math.random(1, 65535)

        sc_srv.on_message = Test.cb(function(self, recv_msg_id, buffer, size)
            Test.equal(recv_msg_id, msg_id)
            Test.equal(size, pingpong_size)

            self:send_pkt(msg_id, pingpong_ud, pingpong_size)
        end)
        sc_clt.on_message = Test.cb(function(self, recv_msg_id, buffer, size)
            Test.equal(recv_msg_id, msg_id)
            Test.equal(size, pingpong_size)
            count = count + 1

            if count >= PERF_TIMES then
                Test.done()
            else
                sc_clt:send_pkt(msg_id, pingpong_ud, pingpong_size)
            end
        end)

        -- 这里本来想做类似于nginx做0kb 1kb 10kb 100kb下性能测试，但由于这里收发只能跑
        -- 在一个线程，没啥意义，所以只测试准确性
        sc_clt:send_pkt(msg_id, pingpong_ud, pingpong_size)

        Test.wait()
    end)

    Test.it("server socket test", function()
        local src = math.random(1, 65535)
        local dst = math.random(1, 65535)
        local mtype = math.random(1, 65535)
        local usize = math.random(1, 65535)

        local str = "1234567890abc"
        local buffer = Buffer()
        local b = buffer:fromstring(str)

        local case_list = {
            {usize}, -- 无udata
            {string.len(str), b}, -- 有udata
        }

        local srv_idx = 0
        local clt_idx = 0
        ss_srv.on_message = function(self, rsrc, rdst, rmtype, rusize, rudata)
            Test.equal(rsrc, src)
            Test.equal(rdst, dst)
            Test.equal(rmtype, mtype)

            srv_idx = srv_idx + 1
            local case = case_list[srv_idx]

            Test.equal(rusize, case[1])
            if not case[2] then
                Test.equal(rudata, nil)
            else
                Test.equal(Buffer.lightud_tostring(rudata, rusize), str)
            end

            self:send_pkt(rsrc, rdst, rmtype, rusize, rudata)
        end
        ss_clt.on_message = function(self, rsrc, rdst, rmtype, rusize, rudata)
            Test.equal(rsrc, src)
            Test.equal(rdst, dst)
            Test.equal(rmtype, mtype)

            clt_idx = clt_idx + 1
            local case = case_list[clt_idx]

            Test.equal(rusize, case[1])
            if not case[2] then
                Test.equal(rudata, nil)
            else
                Test.equal(Buffer.lightud_tostring(rudata, rusize), str)
            end

            if clt_idx == #case_list then Test.done() end
        end

        for _, case in pairs(case_list) do
            ss_clt:send_pkt(src, dst, mtype, case[1], case[2])
        end

        Test.wait()
    end)

    Test.it(string.format("server socket pingpong test %d", PERF_TIMES), function()
        local count = 0
        local src = math.random(1, 65535)
        local dst = math.random(1, 65535)
        local mtype = math.random(1, 65535)

        ss_srv.on_message = function(self, rsrc, rdst, rmtype, rusize, rudata)
            Test.equal(rmtype, mtype)

            self:send_pkt(rsrc, rdst, rmtype, rusize, rudata)
        end
        ss_clt.on_message = function(self, rsrc, rdst, rmtype, rusize, rudata)
            Test.equal(rmtype, rmtype)

            count = count + 1
            if count == PERF_TIMES then
                Test.done()
            else
                ss_clt:send_pkt(src, dst, mtype, pingpong_size, pingpong_ud)
            end
        end
        ss_clt:send_pkt(src, dst, mtype, pingpong_size, pingpong_ud)

        Test.wait(4000)
    end)

    Test.after(function()
        if sc_clt then sc_clt:close() end
        if sc_srv then sc_srv:close() end
        if sc_listen then sc_listen:close() end
        if ss_clt then ss_clt:close() end
        if ss_srv then ss_srv:close() end
        if ss_listen then ss_listen:close() end
        _G.pingpong_b = nil
    end)
end)
