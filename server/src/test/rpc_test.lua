-- rpc调用测试
local network_mgr = network_mgr
local SrvConn = require "network.srv_conn"
g_conn_mgr = require "network.conn_mgr"
local rpc = require "rpc.rpc"

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
        t_wait(2000)
        local params = {
            true,
            0x7FFFFFFFFFFFFFFF,
            "strrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrr",
            0xFFFFFFFFFFFFFFFF,
            false,
            9999999999.5555555,
            nil,
            1000,
            {
                true,
                0x7FFFFFFFFFFFFFFF,
                "strrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrr",
                0xFFFFFFFFFFFFFFFF,
                false,
                9999999999.5555555,
                nil,
                1000,
                {1, true, nil, "str", 9.009, false, {}}
            }
        }
        local counter = 0
        local rpc_empty_query = function(p1)
            t_equal(p1, nil)
            counter = counter + 1
        end
        local rpc_query = function(p1, p2, p3, p4, p5, p6, p7, p8, p9)
            t_equal({p1, p2, p3, p4, p5, p6, p7, p8, p9}, params)
            counter = counter + 1
            return p1, p2, p3, p4, p5, p6, p7, p8, p9
        end

        local rpc_reponse = function(p1, p2, p3, p4, p5, p6, p7, p8, p9, ...)
            counter = counter + 1
            t_equal(rpc:last_error(), 0)
            t_equal({p1, p2, p3, p4, p5, p6, p7, p8, p9}, params)
            t_equal({...}, params)
        end

        local rpc_min_reponse = function(p1, p2, p3, ...)
            t_equal(rpc:last_error(), 0)
            t_equal({...}, params)
            t_equal(p1, params[1])
            t_equal(p2, params[2])
            t_equal(p3, params[3])
            counter = counter + 1
        end

        local rpc_empty_reponse = function(...)
            t_equal(rpc:last_error(), 0)
            t_equal({...}, params)

            -- 最后一个函数执行完，校验结果并结束测试
            t_equal(counter, 3)
            t_done()
        end

        reg_func("rpc_query", rpc_query)
        reg_func("rpc_reponse", rpc_reponse)
        reg_func("rpc_empty_query", rpc_empty_query)
        reg_func("rpc_min_reponse", rpc_min_reponse)
        reg_func("rpc_empty_reponse", rpc_empty_reponse)

        -- 无返回，无参数
        -- rpc:conn_call(clt_conn, rpc_empty_query)
        -- 无返回，有参数
        -- rpc:conn_call(clt_conn, rpc_query, table.unpack(params))
        -- 有返回，有回调参数
        rpc:proxy(rpc_reponse, table.unpack(params)):conn_call(
            clt_conn, rpc_query, table.unpack(params))
        -- 有返回，有少量回调参数
        -- rpc:proxy(rpc_min_reponse, params[1], params[2], params[3]):conn_call(
        --     clt_conn, rpc_query, table.unpack(params))
        -- 有返回，无回调参数
        -- rpc:proxy(rpc_empty_reponse):conn_call(
        --     clt_conn, rpc_query, table.unpack(params))
    end)
    t_it("rpc performance test", function()
        -- 不等待返回，直接发送
    end)

    t_it("rpc pingpong performance test", function()
        -- 一来一回进行测试
    end)

    t_after(function()
        srv_conn:close()
        clt_conn:close()
        listen_conn:close()
    end)
end)