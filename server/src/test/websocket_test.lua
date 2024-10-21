-- websocket_performance.lua
-- 2017-11-29
-- xzc
-- https://tools.ietf.org/pdf/rfc6455.pdf section1.3 page6
-- example at: https://www.websocket.org/aboutwebsocket.html

local WsConn = require "network.ws_conn"
local TlsCtx = require "engine.TlsCtx"

local ws_default_param = WsConn.default_param

t_describe("websocket test", function()
    --[[
    http://websockets.org/echo.html 这个已经不维护了

    https://www.piesocket.com/websocket-tester 可以免费用，但是它的api_key需要每星期手动更换一次
    ]]
    local exp_host = "demo.piesocket.com"
    local exp_url =
        "/v3/channel_1?api_key=VCXCEuvhGcBDP7XhiJJUDvR1e1D3eiVjgZ9VRiaV&notify_self"

    local local_port = 8083
    local local_port_s = 8084
    local local_host = "::1"
    if IPV4 then local_host = "127.0.0.1" end

    local clt_ssl
    local srv_ssl

    t_before(function()
        clt_ssl = TlsCtx()
        srv_ssl = TlsCtx()

        clt_ssl:init()
        srv_ssl:init("../certs/server.cer",
            "../certs/srv_key.pem", "mini_distributed_game_server")
    end)

    t_it("websocket " .. exp_host, function()
        t_async(5000)

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

        local conn = WsConn()
        conn.default_param = ws_default_param
        conn.url = exp_url
        conn:connect(exp_host, 80)

        conn.on_disconnected = function() end
        conn.on_connected = function(self)
            WsConn.send_pkt(self, pkt_body[1])
            self:send_ctrl(self.WS_OP_PING)
            self:send_ctrl(self.WS_OP_PING, ping_body[2])
        end
        conn.on_cmd = function(self, cmd_body)
            pkt_idx = pkt_idx + 1
            t_equal(cmd_body, pkt_body[pkt_idx])

            if pkt_body[pkt_idx + 1] then
                self:send_pkt(pkt_body[pkt_idx + 1])
            end

            check_done(self)
        end

        conn.on_ctrl = function(self, flag, body)
            if flag == self.WS_OP_PONG then
                ping_idx = ping_idx + 1
                t_equal(body, ping_body[ping_idx])
                check_done(self)
            end
        end
    end)

    t_it("websocket ssl " .. exp_host, function()
        t_async(5000)

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

        local conn = WsConn()
        conn.default_param = ws_default_param
        conn.url = exp_url
        conn:connect_s(exp_host, 443, clt_ssl)

        conn.on_disconnected = function() end
        conn.on_connected = function(self)
            WsConn.send_pkt(self, pkt_body[1])
            self:send_ctrl(self.WS_OP_PING)
            self:send_ctrl(self.WS_OP_PING, ping_body[2])
        end
        conn.on_cmd = function(self, cmd_body)
            pkt_idx = pkt_idx + 1
            t_equal(cmd_body, pkt_body[pkt_idx])

            if pkt_body[pkt_idx + 1] then
                self:send_pkt(pkt_body[pkt_idx + 1])
            end

            check_done(self)
        end

        conn.on_ctrl = function(self, flag, body)
            if flag == self.WS_OP_PONG then
                ping_idx = ping_idx + 1
                t_equal(body, ping_body[ping_idx])
                check_done(self)
            end
        end
    end)

    t_it("websocket_local", function()
        t_async(5000)

        local pkt_idx = 0
        local pkt_body = {
            "MServer say hello",
            "MServer say hello2",
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

        listen_conn = WsConn()
        listen_conn.default_param = ws_default_param
        listen_conn:listen(local_host, local_port)
        listen_conn.on_accepted = function(self)
            srv_conn = self
        end
        listen_conn.on_cmd = function(self, cmd_body)
            WsConn.send_pkt(self, cmd_body)
        end
        listen_conn.on_disconnected = function() end

        local conn = WsConn()
        conn.default_param = ws_default_param
        conn:connect(local_host, local_port)

        conn.on_disconnected = function() end
        conn.on_connected = function(self)
            WsConn.send_pkt(self, pkt_body[1])
            self:send_ctrl(self.WS_OP_PING)
            self:send_ctrl(self.WS_OP_PING, ping_body[2])
        end
        conn.on_cmd = function(self, cmd_body)
            pkt_idx = pkt_idx + 1
            t_equal(cmd_body, pkt_body[pkt_idx])

            if pkt_body[pkt_idx + 1] then
                self:send_pkt(pkt_body[pkt_idx + 1])
            end

            check_done(self)
        end

        conn.on_ctrl = function(self, flag, body)
            if flag == self.WS_OP_PONG then
                ping_idx = ping_idx + 1
                t_equal(body, ping_body[ping_idx])
                check_done(self)
            end
        end
    end)

    t_it("websocket ssl local", function()
        t_async(5000)

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

        listen_conn = WsConn()
        listen_conn.default_param = ws_default_param
        listen_conn:listen_s(local_host, local_port_s, srv_ssl)
        listen_conn.on_accepted = function(self)
            srv_conn = self
        end
        listen_conn.on_cmd = function(self, cmd_body)
            WsConn.send_pkt(self, cmd_body)
        end
        listen_conn.on_disconnected = function() end

        local conn = WsConn()
        conn.default_param = ws_default_param
        conn:connect_s(local_host, local_port_s, clt_ssl)

        conn.on_disconnected = function() end
        conn.on_connected = function(self)
            WsConn.send_pkt(self, pkt_body[1])
            self:send_ctrl(self.WS_OP_PING)
            self:send_ctrl(self.WS_OP_PING, ping_body[2])
        end
        conn.on_cmd = function(self, cmd_body)
            pkt_idx = pkt_idx + 1
            t_equal(cmd_body, pkt_body[pkt_idx])

            if pkt_body[pkt_idx + 1] then
                self:send_pkt(pkt_body[pkt_idx + 1])
            end

            check_done(self)
        end

        conn.on_ctrl = function(self, flag, body)
            if flag == self.WS_OP_PONG then
                ping_idx = ping_idx + 1
                t_equal(body, ping_body[ping_idx])
                check_done(self)
            end
        end

    end)
end)
