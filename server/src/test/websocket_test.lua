-- websocket_performance.lua
-- 2017-11-29
-- xzc
-- https://tools.ietf.org/pdf/rfc6455.pdf section1.3 page6
-- example at: https://www.websocket.org/aboutwebsocket.html

local WsConn = require "network.ws_conn"
local CsWsConn = require "network.cs_ws_conn"
local ScWsConn = require "network.sc_ws_conn"

-- 重新建立两个类，避免修改到原来的类，原来的类在其他测试中还要用的
local CltWsConn = oo.class("CltWsConn", CsWsConn)
local SrvWsConn = oo.class("SrvWsConn", ScWsConn)

CltWsConn.default_param = WsConn.default_param
SrvWsConn.default_param = WsConn.default_param

t_describe("websocket test", function()
    --[[
    https://stackoverflow.com/questions/4092591/websocket-live-server
    Your best bet is going to be Kaazing's websockets echo server: http://websockets.org/echo.html. It's easy to remember, they keep it up to date and running.

    ws://echo.websocket.org (port 80)
    wss://echo.websocket.org (port 443)
    EDIT: If you want to use wss:// (443) visit the site with https:// or else use http:// for ws:// (80).
    ]]
    local exp_host = "echo.websocket.org"

    local local_port = 8083
    local local_port_s = 8084
    local local_host = "::1"
    if IPV4 then local_host = "127.0.0.1" end

    local clt_ssl
    local srv_ssl

    t_before(function()
        clt_ssl = network_mgr:new_ssl_ctx(network_mgr.SSLVT_TLS_CLT_AT)
        srv_ssl = network_mgr:new_ssl_ctx(network_mgr.SSLVT_TLS_SRV_AT,
                                        "../certs/server.cer",
                                        "../certs/srv_key.pem",
                                        "mini_distributed_game_server")
    end)

    t_it("websocket " .. exp_host, function()
        t_wait(5000)

        local pkt_idx = 0
        local pkt_body = {
            "MServer send hello",
            "MServer send hello2",
        }

        local ping_idx = 0
        local ping_body = {
            [2] = "ping with body"
        }

        local function check_done(conn)
            if not ping_body[ping_idx + 1] and not pkt_body[pkt_idx + 1] then
                t_done()
                conn:close()
            end
        end

        CltWsConn.conn_ok = function(self)
            WsConn.send_pkt(self, pkt_body[1])
            self:send_ctrl(self.WS_OP_PING)
            self:send_ctrl(self.WS_OP_PING, ping_body[2])
        end
        CltWsConn.command_new = function(self, cmd_body)
            pkt_idx = pkt_idx + 1
            t_equal(cmd_body, pkt_body[pkt_idx])

            if pkt_body[pkt_idx + 1] then
                WsConn.send_pkt(self, pkt_body[pkt_idx + 1])
            end

            check_done(self)
        end

        CltWsConn.on_ctrl = function(self, flag, body)
            if flag == self.WS_OP_PONG then
                ping_idx = ping_idx + 1
                t_equal(body, ping_body[ping_idx])
                check_done(self)
            end
        end

        local conn = CltWsConn()
        conn:connect(exp_host, 80)
    end)

    t_it("websocket ssl " .. exp_host, function()
        t_wait(5000)

        local pkt_idx = 0
        local pkt_body = {
            "MServer ssl send hello",
            "MServer ssl send hello2",
        }

        local ping_idx = 0
        local ping_body = {
            [2] = "ping ssl with body"
        }

        local function check_done(conn)
            if not ping_body[ping_idx + 1] and not pkt_body[pkt_idx + 1] then
                t_done()
                conn:close()
            end
        end

        CltWsConn.conn_ok = function(self)
            WsConn.send_pkt(self, pkt_body[1])
            self:send_ctrl(self.WS_OP_PING)
            self:send_ctrl(self.WS_OP_PING, ping_body[2])
        end
        CltWsConn.command_new = function(self, cmd_body)
            pkt_idx = pkt_idx + 1
            t_equal(cmd_body, pkt_body[pkt_idx])

            if pkt_body[pkt_idx + 1] then
                WsConn.send_pkt(self, pkt_body[pkt_idx + 1])
            end

            check_done(self)
        end

        CltWsConn.on_ctrl = function(self, flag, body)
            if flag == self.WS_OP_PONG then
                ping_idx = ping_idx + 1
                t_equal(body, ping_body[ping_idx])
                check_done(self)
            end
        end

        local conn = CltWsConn()
        conn:connect_s(exp_host, 443, clt_ssl)
    end)

    t_it("websocket local", function()
        t_wait(5000)

        local pkt_idx = 0
        local pkt_body = {
            "MServer send hello",
            "MServer send hello2",
        }

        local ping_idx = 0
        local ping_body = {
            [2] = "ping with body"
        }

        local srv_conn
        local listen_conn

        local function check_done(conn)
            if not ping_body[ping_idx + 1] and not pkt_body[pkt_idx + 1] then
                t_done()
                conn:close()
                srv_conn:close()
                listen_conn:close()
            end
        end

        CltWsConn.conn_ok = function(self)
            WsConn.send_pkt(self, pkt_body[1])
            self:send_ctrl(self.WS_OP_PING)
            self:send_ctrl(self.WS_OP_PING, ping_body[2])
        end
        CltWsConn.command_new = function(self, cmd_body)
            pkt_idx = pkt_idx + 1
            t_equal(cmd_body, pkt_body[pkt_idx])

            if pkt_body[pkt_idx + 1] then
                WsConn.send_pkt(self, pkt_body[pkt_idx + 1])
            end

            check_done(self)
        end

        CltWsConn.on_ctrl = function(self, flag, body)
            if flag == self.WS_OP_PONG then
                ping_idx = ping_idx + 1
                t_equal(body, ping_body[ping_idx])
                check_done(self)
            end
        end

        SrvWsConn.conn_accept = function(self, new_conn_id)
            srv_conn = SrvWsConn(new_conn_id)

            srv_conn:set_conn_param()

            return srv_conn
        end
        SrvWsConn.command_new = function(self, cmd_body)
            WsConn.send_pkt(self, cmd_body)
        end

        listen_conn = SrvWsConn()
        listen_conn:listen(local_host, local_port)

        local conn = CltWsConn()
        conn:connect(local_host, local_port)
    end)

    t_it("websocket local ssl", function()
        t_wait(5000)

        local pkt_idx = 0
        local pkt_body = {
            "MServer send hello",
            "MServer send hello2",
        }

        local ping_idx = 0
        local ping_body = {
            [2] = "ping with body"
        }

        local srv_conn
        local listen_conn

        local function check_done(conn)
            if not ping_body[ping_idx + 1] and not pkt_body[pkt_idx + 1] then
                t_done()
                conn:close()
                srv_conn:close()
                listen_conn:close()
            end
        end

        CltWsConn.conn_ok = function(self)
            WsConn.send_pkt(self, pkt_body[1])
            self:send_ctrl(self.WS_OP_PING)
            self:send_ctrl(self.WS_OP_PING, ping_body[2])
        end
        CltWsConn.command_new = function(self, cmd_body)
            pkt_idx = pkt_idx + 1
            t_equal(cmd_body, pkt_body[pkt_idx])

            if pkt_body[pkt_idx + 1] then
                WsConn.send_pkt(self, pkt_body[pkt_idx + 1])
            end

            check_done(self)
        end

        CltWsConn.on_ctrl = function(self, flag, body)
            if flag == self.WS_OP_PONG then
                ping_idx = ping_idx + 1
                t_equal(body, ping_body[ping_idx])
                check_done(self)
            end
        end

        SrvWsConn.conn_accept = function(self, new_conn_id)
            srv_conn = SrvWsConn(new_conn_id)

            srv_conn.ssl = self.ssl
            srv_conn:set_conn_param()

            return srv_conn
        end
        SrvWsConn.command_new = function(self, cmd_body)
            WsConn.send_pkt(self, cmd_body)
        end

        listen_conn = SrvWsConn()
        listen_conn:listen_s(local_host, local_port_s, srv_ssl)

        local conn = CltWsConn()
        conn:connect_s(local_host, local_port_s, clt_ssl)
    end)
end)
