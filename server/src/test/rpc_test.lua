-- rpc调用测试
local network_mgr = network_mgr
local SrvConn = require "network.srv_conn"
g_conn_mgr = require "network.conn_mgr"

local srv_conn = nil
local clt_conn = nil
local listen_conn = nil

-- 执行单元测试时，不需要链接执行握手，所以需要重载SrvConn中的conn_accept等函数
local RpcConn = oo.class("RpcConn", SrvConn)

function RpcConn:conn_accept(new_conn_id)
    self:set_conn_param(new_conn_id)
    srv_conn = RpcConn(new_conn_id)
    return srv_conn
end

function RpcConn:conn_del()
    g_conn_mgr:set_conn(self.conn_id, nil)
end

function RpcConn:conn_new(ecode)
    t_equal(ecode, 0)

    self:set_conn_param(self.conn_id)
end
function RpcConn:conn_del()
    g_conn_mgr:set_conn(self.conn_id, nil)
end

function RpcConn:conn_ok()
    if self == clt_conn then t_done() end
end

-- /////////////////////////////////////////////////////////////////////////////

t_describe("rpc test", function()
    local local_host = "::1"
    local local_port = 1099
    if IPV4 then local_host = "127.0.0.1" end

    -- 执行单元测试时，不需要链接执行握手，所以需要重载SrvConn中的conn_accept等函数
    t_before(function()
        clt_conn = RpcConn()
        listen_conn = RpcConn()

        listen_conn:listen(local_host, local_port)
        clt_conn:connect(local_host, local_port)
        t_wait(2000)
    end)
    t_it("rpc base test", function()
        PRINTF(listen_conn.conn_id)
        PRINTF(clt_conn.conn_id)
        PRINTF(srv_conn.conn_id)
    end)
    t_it("rpc performance test", function()
    end)

    t_after(function()
        srv_conn:close()
        clt_conn:close()
        listen_conn:close()
    end)
end)
