-- protobuf协议测试

g_authorize = require "modules.system.authorize"
g_stat_mgr = require "statistic.statistic_mgr"

require "network.cmd"
local ScConn = require "network.sc_conn"
local CsConn = require "network.cs_conn"

local CsWsConn = require "network.cs_ws_conn"
local ScWsConn = require "network.sc_ws_conn"

-- 重新建立两个类，避免修改到原来的类，原来的类在其他测试中还要用的
local CltWsConn = oo.class("CltWsConn_0", CsWsConn)
local SrvWsConn = oo.class("SrvWsConn_0", ScWsConn)

local srv_conn = nil
local clt_conn = nil
local listen_conn = nil

local ProtobufConn = oo.class("ProtobufConn", ScConn)

function ProtobufConn:conn_accept(new_conn_id)
    srv_conn = ProtobufConn(new_conn_id)

    srv_conn:set_conn_param()
    return srv_conn
end

function ProtobufConn:conn_new(ecode)
    t_equal(ecode, 0)

    self:set_conn_param()
end
function ProtobufConn:conn_del()
end

function ProtobufConn:conn_ok()
end

function CsConn:conn_ok()
    t_done()
end

t_describe("protobuf test", function()
    local local_host = "::1"
    local local_port = 2099
    if IPV4 then local_host = "127.0.0.1" end

    local local_port_ws = 8085
    local local_port_wss = 8086

    local PERF_TIMES = 1000

    local clt_ssl
    local srv_ssl

    -- https://stackoverflow.com/questions/63821960/lua-odd-min-integer-number
    -- -9223372036854775808在lua中会被解析为一个number而不是整型
    -- 使用 -9223372036854775808|0 或者 math.mininteger

    local base_pkt = nil -- 基准测试用的包，主要用于各种临界条件测试
    local lite_pkt = nil -- 简单的测试包，更接近于日常通信用的包
    local rep_cnt = 512 -- 512的时候，整个包达到50000多字节了
    t_before(function()
        clt_ssl = network_mgr:new_ssl_ctx(network_mgr.SSLVT_TLS_CLT_AT)
        srv_ssl = network_mgr:new_ssl_ctx(network_mgr.SSLVT_TLS_SRV_AT,
                                        "../certs/server.cer",
                                        "../certs/srv_key.pem",
                                        "mini_distributed_game_server")

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

        -- 加载协议文件
        local ok = Cmd.load_protobuf()
        t_equal(ok, true)

        -- 建立一个客户端、一个服务端连接，模拟通信
        clt_conn = CsConn()
        listen_conn = ProtobufConn()

        listen_conn:listen(local_host, local_port)
        clt_conn:connect(local_host, local_port)

        t_wait(2000)
    end)
    t_it("protobuf base", function()
        Cmd.reg(PLAYER.PING, function(conn, pkt)
            t_equal(pkt, base_pkt)
            conn:send_pkt(PLAYER.PING, pkt)
        end, true)
        clt_conn.command_new = function(self, cmd, e, pkt)
            t_equal(cmd, PLAYER.PING.i)
            t_equal(pkt, base_pkt)
            t_done()
        end

        clt_conn:send_pkt(PLAYER.PING, base_pkt)

        t_wait()
    end)
    t_it(string.format("protobuf performance test %d", PERF_TIMES), function()
        local count = 0
        Cmd.reg(PLAYER.PING_LITE, function(conn, pkt)
            conn:send_pkt(PLAYER.PING_LITE, pkt)
        end, true)
        clt_conn.command_new = function(self, cmd, e, pkt)
            t_equal(cmd, PLAYER.PING_LITE.i)
            count = count + 1

            if count >= PERF_TIMES then t_done() end
        end

        -- 一次性发送大量数据，测试缓冲区及打包效率
        -- 由于大量的包堆在缓冲区，需要用到多个缓冲区块，有很多using continuous buffer日志
        for _ = 1, PERF_TIMES do
            clt_conn:send_pkt(PLAYER.PING_LITE, lite_pkt)
        end

        t_wait()
    end)
    t_it(string.format("protobuf pingpong test %d", PERF_TIMES), function()
        local count = 0
        Cmd.reg(PLAYER.PING_LITE, function(conn, pkt)
            conn:send_pkt(PLAYER.PING_LITE, pkt)
        end, true)
        clt_conn.command_new = function(self, cmd, e, pkt)
            t_equal(cmd, PLAYER.PING_LITE.i)
            count = count + 1

            if count >= PERF_TIMES then
                t_done()
            else
                clt_conn:send_pkt(PLAYER.PING_LITE, lite_pkt)
            end
        end

        -- 测试来回发送数据，这个取决于打包、传输效率
        clt_conn:send_pkt(PLAYER.PING_LITE, lite_pkt)

        t_wait()
    end)
    t_it(string.format("protobuf ws perf test %d", PERF_TIMES), function()
        local count = 0
        local srv_conn_ws
        local clt_conn_ws
        local listen_conn_ws

        Cmd.reg(PLAYER.PING_LITE, function(conn, pkt)
            count = count + 1

            if count >= PERF_TIMES then
                srv_conn_ws:close()
                clt_conn_ws:close()
                listen_conn_ws:close()
                t_done()
            end
        end, true)
        CltWsConn.conn_ok = function(self)
            for _ = 1, PERF_TIMES do
                self:send_pkt(PLAYER.PING_LITE, lite_pkt)
            end
        end
        SrvWsConn.conn_accept = function(self, new_conn_id)
            srv_conn_ws = SrvWsConn(new_conn_id)

            srv_conn_ws:set_conn_param()

            return srv_conn_ws
        end

        listen_conn_ws = SrvWsConn()
        listen_conn_ws:listen(local_host, local_port_ws)

        clt_conn_ws = CltWsConn()
        clt_conn_ws:connect(local_host, local_port_ws)

        t_wait()
    end)

    t_it(string.format("protobuf ws pingpong perf test %d", PERF_TIMES), function()
        local count = 0
        local srv_conn_ws
        local clt_conn_ws
        local listen_conn_ws

        Cmd.reg(PLAYER.PING_LITE, function(conn, pkt)
            conn:send_pkt(PLAYER.PING_LITE, pkt)
        end, true)
        CltWsConn.conn_ok = function(self)
            self:send_pkt(PLAYER.PING_LITE, lite_pkt)
        end
        CltWsConn.command_new = function(self, cmd, e, pkt)
            t_equal(cmd, PLAYER.PING_LITE.i)
            count = count + 1
            if count >= PERF_TIMES then
                srv_conn_ws:close()
                clt_conn_ws:close()
                listen_conn_ws:close()
                t_done()
            else
                self:send_pkt(PLAYER.PING_LITE, lite_pkt)
            end
        end
        SrvWsConn.conn_accept = function(self, new_conn_id)
            srv_conn_ws = SrvWsConn(new_conn_id)

            srv_conn_ws:set_conn_param()

            return srv_conn_ws
        end

        listen_conn_ws = SrvWsConn()
        listen_conn_ws:listen(local_host, local_port_ws)

        clt_conn_ws = CltWsConn()
        clt_conn_ws:connect(local_host, local_port_ws)

        t_wait()
    end)

    t_it(string.format("protobuf wss perf test %d", PERF_TIMES), function()
        local count = 0
        local srv_conn_ws
        local clt_conn_ws
        local listen_conn_ws

        Cmd.reg(PLAYER.PING_LITE, function(conn, pkt)
            count = count + 1

            if count >= PERF_TIMES then
                srv_conn_ws:close()
                clt_conn_ws:close()
                listen_conn_ws:close()
                t_done()
            end
        end, true)
        CltWsConn.conn_ok = function(self)
            for _ = 1, PERF_TIMES do
                self:send_pkt(PLAYER.PING_LITE, lite_pkt)
            end
        end
        SrvWsConn.conn_accept = function(self, new_conn_id)
            srv_conn_ws = SrvWsConn(new_conn_id)

            srv_conn_ws.ssl = self.ssl
            srv_conn_ws:set_conn_param()

            return srv_conn_ws
        end

        listen_conn_ws = SrvWsConn()
        listen_conn_ws:listen_s(local_host, local_port_wss, srv_ssl)

        clt_conn_ws = CltWsConn()
        clt_conn_ws:connect_s(local_host, local_port_wss, clt_ssl)

        t_wait()
    end)

    t_after(function()
        clt_conn:close()
        srv_conn:close()
        listen_conn:close()
    end)
end)
