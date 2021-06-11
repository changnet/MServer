-- rpc调用测试
local network_mgr = network_mgr
local SrvConn = require "network.srv_conn"
g_conn_mgr = require "network.conn_mgr"

t_describe("rpc test", function()
    local srv_conn = nil
    local clt_conn = nil
    local listen_conn = nil

    local local_host = "::1"
    local local_port = 1099
    if IPV4 then local_host = "127.0.0.1" end

    -- 
    t_before(function()
        t_wait(2000)
        listen_conn = SrvConn()
        listen_conn:listen(local_host, local_port)

        clt_conn = SrvConn()
        clt_conn:connect(local_host, local_port)
    end)
    t_it("rpc base test", function()
    end)
    t_it("rpc performance test", function()
    end)

    t_after(function()
        srv_conn:close()
        clt_conn:close()
        listen_conn:close()
    end)
end)
