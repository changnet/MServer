-- rpc调用测试

g_stat_mgr = require "statistic.statistic_mgr"
local SsConn = require "network.ss_conn"
local rpc = require "rpc.rpc"

local srv_conn = nil
local clt_conn = nil
local listen_conn = nil

-- /////////////////////////////////////////////////////////////////////////////

t_describe("rpc test", function()
    local local_host = "::1"
    local local_port = 1099
    if IPV4 then local_host = "127.0.0.1" end

    local PERF_TIMES = 1000
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

    -- 执行单元测试时，不需要链接执行握手，所以需要重载SrvConn中的conn_accept等函数
    t_before(function()
        listen_conn = SsConn()
        listen_conn:listen(local_host, local_port)
        listen_conn.on_accepted = function(self)
            srv_conn = self
        end
        listen_conn.on_connected = function(self) end

        clt_conn = SsConn()
        clt_conn:connect(local_host, local_port)
        clt_conn.on_connected = function(self)
            t_done()
        end

        t_wait(2000)
    end)
    t_it("rpc base test", function()
        t_wait(2000)

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
            counter = counter + 1

            -- 最后一个函数执行完，校验结果并结束测试
            -- 8 = 两次单向(2) + 3次双向(3 * 2)
            t_equal(counter, 8)
            t_done()
        end

        reg_func("rpc_query", rpc_query)
        reg_func("rpc_reponse", rpc_reponse)
        reg_func("rpc_empty_query", rpc_empty_query)
        reg_func("rpc_min_reponse", rpc_min_reponse)
        reg_func("rpc_empty_reponse", rpc_empty_reponse)

        -- 无返回，无参数
        rpc:conn_call(clt_conn, rpc_empty_query)
        -- 无返回，有参数
        rpc:conn_call(clt_conn, rpc_query, table.unpack(params))
        -- 有返回，有回调参数
        rpc:proxy(rpc_reponse, table.unpack(params)):conn_call(
            clt_conn, rpc_query, table.unpack(params))
        -- 有返回，有少量回调参数
        rpc:proxy(rpc_min_reponse, params[1], params[2], params[3]):conn_call(
            clt_conn, rpc_query, table.unpack(params))
        -- 有返回，无回调参数
        rpc:proxy(rpc_empty_reponse):conn_call(
            clt_conn, rpc_query, table.unpack(params))
    end)
    t_it(string.format("rpc performance test %d", PERF_TIMES), function()
        t_wait(5000)

        -- 不等待返回，直接发送，这种其实是取决于缓冲区

        local count = 0
        local rpc_perf_query = function(...)
            return ...
        end
        local rpc_perf_response = function(...)
            count = count + 1
            if count == PERF_TIMES then t_done() end
        end

        reg_func("rpc_perf_query", rpc_perf_query)
        reg_func("rpc_perf_response", rpc_perf_response)

        for _ = 1, PERF_TIMES do
            rpc:proxy(rpc_perf_response):conn_call(
                clt_conn, rpc_perf_query, table.unpack(params))
        end
    end)

    t_it(string.format("rpc pingpong performance test %d", PERF_TIMES),
        function()
            t_wait(5000)
            -- 一来一回进行测试
            local count = 0
            local rpc_pingpong_query = function(...)
                return ...
            end

            local rpc_pingpong_response = nil -- luacheck要前置声明
            rpc_pingpong_response = function(...)
                count = count + 1
                if count == PERF_TIMES then
                    t_done()
                else
                    rpc:proxy(rpc_pingpong_response):conn_call(
                        clt_conn, rpc_pingpong_query, table.unpack(params))
                end
            end

            reg_func("rpc_pingpong_query", rpc_pingpong_query)
            reg_func("rpc_pingpong_response", rpc_pingpong_response)

            rpc:proxy(rpc_pingpong_response):conn_call(
                clt_conn, rpc_pingpong_query, table.unpack(params))
        end
    )

    t_after(function()
        srv_conn:close()
        clt_conn:close()
        listen_conn:close()
    end)
end)
