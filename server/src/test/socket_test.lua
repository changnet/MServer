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

local TEST_IPV4 = false

-- 找一个本机的link-local地址(fe80::/10)。
-- link-local只在一条链路上有效，每块网卡都各有一个，发数据时必须带上%接口(scope)，
-- 所以返回的字符串形如 "fe80::b50a:d267:b9a7:7209%7"。
-- 每台机器的地址都不一样，也可以用 --lladdr 手工指定，如 --lladdr "fe80::1%12"
local function get_link_local_addr()
    local ll = g_sharedata:get("--lladdr")
    if ll and "" ~= ll then return ll end

    local cmd
    if WINDOWS then
        cmd = "ipconfig"
    else
        -- -o 把每个地址压成一行，接口名就在行首
        cmd = "ip -o -6 addr show scope link"
    end

    local pipe = io.popen(cmd)
    if not pipe then return nil end
    local text = pipe:read("a")
    pipe:close()
    if not text then return nil end

    if WINDOWS then
        -- 本地链接 IPv6 地址. . . . . . . . : fe80::b50a:d267:b9a7:7209%7
        return text:match("(fe80:[%x:]+%%%d+)")
    end

    -- 2: eth0    inet6 fe80::xxxx/64 scope link \ ...
    for line in text:gmatch("[^\n]+") do
        local name, addr =
            line:match("^%s*%d+:%s+([%w%._%-]+):?.-inet6%s+(fe80:[%x:]+)/")
        if name and addr then return addr .. "%" .. name end
    end

    return nil
end


Test.describe("socket test", function()
    local sc_srv = nil
    local sc_clt = nil
    local sc_listen = nil
    local ss_srv = nil
    local ss_clt = nil
    local ss_listen = nil

    local udp_srv = nil
    local udp_clt1 = nil
    local udp_clt2 = nil

    local kcp_srv = nil
    local kcp_clt1 = nil
    local kcp_clt2 = nil

    -- link-local是独立的测试，单独一套socket和端口
    local kcp_ll_srv = nil
    local kcp_ll_clt = nil

    local ss_port = 2098
    local sc_port = 2099
    -- kcp跑在udp上，不能和上面的udp测试共用端口
    local kcp_port = 2101
    local kcp_ll_port = 2102
    local local_host = TEST_IPV4 and "127.0.0.1" or "::1"

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

    Test.it("udp socket test", function()
        local count = 10
        local udp_mgr = {}
        local UdpSocket = require "network.udp_socket"

        udp_srv = UdpSocket()
        -- udp没有连接过程，listen失败这里是唯一能发现问题的地方，必须断言
        assert(udp_srv:listen(local_host, ss_port), "udp listen fail")

        -- 服务器的一个UdpSocket对应一个对端，用unpack返回的20字节地址做key
        local function get_clt(addr)
            local clt = udp_mgr[addr]
            if not clt then
                clt = UdpSocket(addr, udp_srv)
                udp_mgr[addr] = clt

                local ip, port = clt:address()
                print(string.format("new udp client %s:%d", ip, port))
            end
            return clt
        end

        udp_srv.on_message = function(self, addr, ud, size)
            get_clt(addr):send_pkt(ud, size)
        end

        local function clt_send(clt, prefix)
            local last_send = prefix .. tostring(math.random(10000, 1000000))

            clt.last_send = last_send
            clt:send_pkt(last_send, #last_send)
        end

        local count1 = 0
        local count2 = 0

        local done = false
        local function check_done()
            -- 只有两个客户端都收到足够的包才算完成，不能提前关闭socket
            if done or count1 < count or count2 < count then return end
            done = true

            Test.done()
        end

        udp_clt1 = UdpSocket()
        udp_clt2 = UdpSocket()

        udp_clt1:connect(local_host, ss_port)
        udp_clt1.on_message = function(self, addr, ud, size)
            local str = Buffer.lightud_tostring(ud, size)
            Test.equal(str, self.last_send)

            count1 = count1 + 1
            if count1 >= count then return check_done() end

            -- print("udp_clt1 recv", str)
            clt_send(self, "clt1")
        end

        udp_clt2:connect(local_host, ss_port)
        udp_clt2.on_message = function(self, addr, ud, size)
            local str = Buffer.lightud_tostring(ud, size)
            Test.equal(str, self.last_send)

            count2 = count2 + 1
            if count2 >= count then return check_done() end

            -- print("udp_clt2 recv", str)
            clt_send(self, "clt2")
        end

        clt_send(udp_clt1, "clt1")
        clt_send(udp_clt2, "clt2")

        Test.wait(4000)
    end)

    Test.it("kcp socket test", function()
        local count = 10
        local KcpSocket = require "network.kcp_socket"

        -- kcp没有连接过程，listen、connect失败这里是唯一能发现问题的地方，必须断言
        kcp_srv = KcpSocket()
        assert(kcp_srv:listen(local_host, kcp_port), "kcp listen fail")

        -- 服务端每个对端都是一条独立连接：数据到达后端之后才建对端，
        -- 由 Socket:on_accepting 创建对象，监听socket上设置的回调会被继承下去。
        -- on_connected 对每个对端调用一次，用它记录服务端建了几条连接
        local srv_peers = {}
        kcp_srv.on_connected = function(self)
            table.insert(srv_peers, self)
        end

        -- kcp的包不带对端地址(一条连接就对应一个对端)，回调参数是 (ud, size)
        kcp_srv.on_message = Test.cb(function(self, ud, size)
            self:send_pkt(ud, size)
        end)

        -- 每轮的内容都不一样(带序号和随机数)，确保回来的是这一轮发出去的数据；
        -- 第5轮用超过mtu(1400)的大包，同时验证kcp的分片重组和lua侧的4字节长度帧
        local big_str = string.rep("0123456789", 800)
        local function clt_send(clt, prefix)
            local idx = (clt.send_count or 0) + 1
            clt.send_count = idx

            local last_send = string.format("%s_%d_%d", prefix, idx,
                                            math.random(10000, 1000000))
            if 5 == idx then last_send = last_send .. big_str end

            clt.last_send = last_send
            clt:send_pkt(last_send, #last_send)
        end

        local count1 = 0
        local count2 = 0

        local done = false
        local function check_done()
            -- 只有两个客户端都收到足够的包才算完成，不能提前关闭socket
            if done or count1 < count or count2 < count then return end
            done = true

            -- 两个客户端两条连接，服务端应该accept出两条
            Test.equal(#srv_peers, 2)

            Test.done()
        end

        kcp_clt1 = KcpSocket()
        kcp_clt1.on_message = Test.cb(function(self, ud, size)
            Test.equal(Buffer.lightud_tostring(ud, size), self.last_send)

            count1 = count1 + 1
            if count1 >= count then return check_done() end

            clt_send(self, "clt1")
        end)
        assert(kcp_clt1:connect(local_host, kcp_port), "kcp connect fail")

        kcp_clt2 = KcpSocket()
        kcp_clt2.on_message = Test.cb(function(self, ud, size)
            Test.equal(Buffer.lightud_tostring(ud, size), self.last_send)

            count2 = count2 + 1
            if count2 >= count then return check_done() end

            clt_send(self, "clt2")
        end)
        assert(kcp_clt2:connect(local_host, kcp_port), "kcp connect fail")

        clt_send(kcp_clt1, "clt1")
        clt_send(kcp_clt2, "clt2")

        -- kcp的flush间隔是40ms，一个来回至少两跳，10轮留够时间
        Test.wait(5000)
    end)

    -- link-local(fe80::/10)只在一条链路上有效，必须带上%接口(scope)才能发出去。
    -- 地址里的scope一旦在哪个环节丢了，要么sendto选错网卡，要么不同链路上同名的
    -- fe80被当成同一个客户端，都是不报错的静默故障，所以单独跑一遍完整收发。
    -- 注意标题里不要写 "link-local"：--filter 是lua模式匹配，"-" 是量词，
    -- 用 --filter "link-local" 会一个测试都匹配不到
    Test.it("kcp link local test", function()
        local ll_addr = get_link_local_addr()
        if not ll_addr then
            -- 机器上没有开启ipv6时不算失败，跳过即可
            print("no link-local ipv6 address found, skip kcp link-local test")
            return Test.done()
        end
        print("kcp link-local test addr", ll_addr)

        local KcpSocket = require "network.kcp_socket"

        -- listen和connect都要能解析 "ip%scope"，scope可以是接口索引或接口名
        kcp_ll_srv = KcpSocket()
        assert(kcp_ll_srv:listen(ll_addr, kcp_ll_port),
               "kcp link-local listen fail: " .. ll_addr)

        local srv_peer = nil
        kcp_ll_srv.on_connected = function(self)
            srv_peer = self
        end
        kcp_ll_srv.on_message = Test.cb(function(self, ud, size)
            self:send_pkt(ud, size)
        end)

        kcp_ll_clt = KcpSocket()
        kcp_ll_clt.on_message = Test.cb(function(self, ud, size)
            Test.equal(Buffer.lightud_tostring(ud, size), self.last_send)
            -- 客户端到达服务端时又会拆出一条独立连接，说明accept也走通了
            Test.assert(srv_peer, "kcp link-local server peer not established")

            Test.done()
        end)
        assert(kcp_ll_clt:connect(ll_addr, kcp_ll_port),
               "kcp link-local connect fail: " .. ll_addr)

        -- 超过mtu(1400)的包，验证kcp分片后每个分片都带着正确的scope发出去
        local msg = string.rep("linklocal-", 200) .. math.random(10000, 1000000)
        kcp_ll_clt.last_send = msg
        kcp_ll_clt:send_pkt(msg, #msg)

        Test.wait(3000)
    end)

    Test.after(function()
        if sc_clt then sc_clt:close() end
        if sc_srv then sc_srv:close() end
        if sc_listen then sc_listen:close() end
        if ss_clt then ss_clt:close() end
        if ss_srv then ss_srv:close() end
        if ss_listen then ss_listen:close() end

        if udp_srv then udp_srv:close() end
        if udp_clt1 then udp_clt1:close() end
        if udp_clt2 then udp_clt2:close() end

        if kcp_clt1 then kcp_clt1:close() end
        if kcp_clt2 then kcp_clt2:close() end
        -- 关监听会把名下的对端一起回收
        if kcp_srv then kcp_srv:close() end

        if kcp_ll_clt then kcp_ll_clt:close() end
        if kcp_ll_srv then kcp_ll_srv:close() end

        _G.pingpong_b = nil
    end)
end)
