-- protobuf协议测试

g_authorize = require "modules.system.authorize"
g_stat_mgr = require "statistic.statistic_mgr"

require "modules.command.command_mgr"
local CltConn = require "network.clt_conn"

local srv_conn = nil
local clt_conn = nil
local listen_conn = nil

local ProtobufConn = oo.class("ProtobufConn", CltConn)

function ProtobufConn:conn_accept(new_conn_id)
    self:set_conn_param(new_conn_id)
    srv_conn = ProtobufConn(new_conn_id)
    return srv_conn
end

function ProtobufConn:conn_del()
    self:set_conn(self.conn_id, nil)
end

function ProtobufConn:conn_new(ecode)
    t_equal(ecode, 0)

    self:set_conn_param(self.conn_id)
end
function ProtobufConn:conn_del()
    self:set_conn(self.conn_id, nil)
end

function ProtobufConn:conn_ok()
    if self == clt_conn then t_done() end
end


t_describe("protobuf test", function()
    local local_host = "::1"
    local local_port = 2099
    if IPV4 then local_host = "127.0.0.1" end

    -- https://stackoverflow.com/questions/63821960/lua-odd-min-integer-number
    -- -9223372036854775808在lua中会被解析为一个number而不是整型
    -- 使用 -9223372036854775808|0 或者 math.mininteger

    local pp_pkt = nil
    t_before(function()
        pp_pkt = {
            d1 = -99999999999999.55555,
            d2 = 99999999999999.55555,
            f1 = -11111111.55555,
            f2 = 11111111.55555,
            i1 = -2147483648;
            i2 = 2147483647;
            i641 = math.mininteger,
            i642 = math.maxinteger,
            b1 = true,
            b2 = false,
            s1 = "",
            s2 = string.rep("ssssssssss", 512),
            by1 = "";
            by2 = string.rep("ssssssssss", 512);
            ui1 = 1,
            ui2 = 4294967295,
            ui641 = 1,
            ui642 = 0xffffffffffffffff,
            -- fix相当于 uint32, sint才是有符号的
            f321 = 2147483648,
            f322 = 2147483647,
            f641 = 1,
            f642 = 0xffffffffffffffff,
            i_list = {1,2,3,4,5,99999,55555,111111111},
        }
        pp_pkt.msg1 = pp_pkt
        pp_pkt.msg_list = { pp_pkt, pp_pkt, pp_pkt}

        -- 加载协议文件
        local ok = Cmd.load_schema(
            network_mgr.CDC_PROTOBUF, "../pb", nil, "pb")
        t_equal(ok, true)

        -- 建立一个客户端、一个服务端连接，模拟通信
        clt_conn = ProtobufConn()
        listen_conn = ProtobufConn()

        listen_conn:listen(local_host, local_port)
        clt_conn:connect(local_host, local_port)

        -- 作为客户端，需要改下接口
        clt_conn.send_pkt = function(self, cmd, pkt)
            return network_mgr:send_srv_packet(self.conn_id, cmd.i, pkt)
        end
        t_wait(2000)
    end)
    t_it("protobuf base", function()
        Cmd.reg(PLAYER.PING, function(conn, pkt)
            vd(pkt)
            t_done()
        end, true)

        clt_conn:send_pkt(PLAYER.PING, pp_pkt)

        t_wait()
    end)
    t_it("protobuf perf", function()
        Cmd.reg(PLAYER.PING, function(conn, pkt)
        end, true)

        -- clt_conn:send_pkt(PLAYER.PING, {
        --     index = 1,
        --     context = "abc"
        -- }, 0)
    end)

    t_after(function()
        clt_conn:close()
        srv_conn:close()
        listen_conn:close()
    end)
end)
