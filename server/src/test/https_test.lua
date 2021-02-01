-- http(s)_test.lua
-- 2017-12-11
-- xzc

local network_mgr = network_mgr
local HttpConn = require "http.http_conn"
g_conn_mgr = require "network.conn_mgr"

t_describe("http(s) test", function()
    -- 产生一个缓存，避免下面连接时查询dns导致测试超时
    -- example.com不稳定，经常连不上，用postman来测试
    -- local exp_host = "www.example.com"
    local exp_host = "postman-echo.com"

    -- 测试时发现，ipv6可以把ipv4的地址传进去
    local local_host = "::1" -- "127.0.0.1"

    local clt_ssl
    local srv_ssl
    local vfy_ssl
    local vfy_srv_ssl
    local vfy_clt_ssl

    t_before( function()
        clt_ssl = network_mgr:new_ssl_ctx(network_mgr.SSLVT_TLS_CLT_AT)

        vfy_ssl = network_mgr:new_ssl_ctx(
            network_mgr.SSLVT_TLS_CLT_AT, nil, nil, nil,
            "/etc/ssl/certs/ca-certificates.crt")

        srv_ssl = network_mgr:new_ssl_ctx(
            network_mgr.SSLVT_TLS_SRV_AT, "../certs/server.cer",
            "../certs/srv_key.pem","mini_distributed_game_server" )

        vfy_srv_ssl = network_mgr:new_ssl_ctx(
            network_mgr.SSLVT_TLS_SRV_AT, "../certs/server.cer",
            "../certs/srv_key.pem","mini_distributed_game_server",
            "../certs/ca.cer" )

        vfy_clt_ssl = network_mgr:new_ssl_ctx(
            network_mgr.SSLVT_TLS_CLT_AT, "../certs/client.cer",
            "../certs/clt_key.pem","mini_distributed_game_server",
            "../certs/ca.cer")

        local ip = util.get_addr_info(exp_host)
        t_print("target host address is", ip)
    end)

    t_it("http get " .. exp_host, function()
        t_wait(10000)

        local conn = HttpConn()

        conn:connect(exp_host, 80, function(_conn, ecode)
            t_equal(ecode, 0)

            conn:get("/get", nil,
                function(__conn, http_type, code, method, url, body)
                    t_equal(http_type, 1)
                    t_equal(code, 200)
                    conn:close()
                    t_done()
                end)
        end)
    end)

    t_it("http post " .. exp_host, function()
        t_wait(10000)

        local conn = HttpConn()

        conn:connect(exp_host, 80, function(_conn, ecode)
            t_equal(ecode, 0)

            conn:post("/post", nil,
                function(__conn, http_type, code, method, url, body)
                    t_equal(http_type, 1)
                    t_equal(code, 200)
                    conn:close()
                    t_done()
                end)
        end)
    end)

    t_it("http local server test", function()
        t_wait(10000)

        local ctx = "hello"

        local port = 8182

        local srvConn = HttpConn()
        srvConn:listen(local_host, port, nil,
            function(conn, http_type, code, method, url, body)
                t_equal(http_type, 0)

                -- 1 = GET, 3 = POST
                if "/get" == url then
                    t_equal(method, 1)
                else
                    t_equal(url, "/post")
                    t_equal(method, 3)
                end

                local addr = network_mgr:address(conn.conn_id)

                t_print("accept http from", addr)
                t_equal(addr, local_host)
                conn:send_pkt(string.format(HTTP.P200, ctx:len(), ctx))
            end)

        local cltConn = HttpConn()
        cltConn:connect(local_host, port, function(_, ecode)
            t_equal(ecode, 0)

            cltConn:get("/get", nil,
                function(_, http_type, code, method, url, body)
                    local header = cltConn:get_header()
                    t_equal(code, 200)
                    t_equal(header.Server, "Mini-Game-Distribute-Server/1.0")

                    cltConn:post("/post", nil,
                        function(_, http_type2, code2, method2, url2, body2)
                            t_equal(code2, 200)
                            cltConn:close()
                            t_done()
                        end)
                end)
        end)
    end)


    t_it("https get " .. exp_host, function()
        t_wait(10000)

        local conn = HttpConn()

        conn:connect_s(exp_host, 443, clt_ssl, function(_conn, ecode)
            t_equal(ecode, 0)

            conn:get("/get", nil,
                function(__conn, http_type, code, method, url, body)
                    t_equal(http_type, 1)
                    t_equal(code, 200)
                    conn:close()
                    t_done()
                end)
        end)
    end)

    t_it("https post " .. exp_host, function()
        t_wait(10000)

        local conn = HttpConn()

        conn:connect_s(exp_host, 443, clt_ssl, function(_conn, ecode)
            t_equal(ecode, 0)

            conn:post("/post", nil,
                function(__conn, http_type, code, method, url, body)
                    t_equal(http_type, 1)
                    t_equal(code, 200)
                    conn:close()
                    t_done()
                end)
        end)
    end)

    t_it("https local server test", function()
        t_wait(10000)

        local ctx = "hello"

        local port = 8183

        local srvConn = HttpConn()
        srvConn:listen_s(local_host, port, srv_ssl, nil,
            function(conn, http_type, code, method, url, body)
                t_equal(http_type, 0)

                -- 1 = GET, 3 = POST
                if "/get" == url then
                    t_equal(method, 1)
                else
                    t_equal(url, "/post")
                    t_equal(method, 3)
                end

                conn:send_pkt(string.format(HTTP.P200, ctx:len(), ctx))
            end)

        local cltConn = HttpConn()
        cltConn:connect_s(local_host, port, clt_ssl, function(_, ecode)
            t_equal(ecode, 0)

            cltConn:get("/get", nil,
                function(_, http_type, code, method, url, body)
                    local header = cltConn:get_header()
                    t_equal(code, 200)
                    t_equal(header.Server, "Mini-Game-Distribute-Server/1.0")

                    cltConn:post("/post", nil,
                        function(_, http_type2, code2, method2, url2, body2)
                            t_equal(code2, 200)
                            cltConn:close()
                            t_done()
                        end)
                end)
        end)
    end)

    t_it("https valify and get " .. exp_host, function()
        t_wait(10000)

        local conn = HttpConn()

        conn:connect_s(exp_host, 443, vfy_ssl, function(_conn, ecode)
            t_equal(ecode, 0)

            conn:get("/get", nil,
                function(__conn, http_type, code, method, url, body)
                    t_equal(http_type, 1)
                    t_equal(code, 200)
                    conn:close()
                    t_done()
                end)
        end)
    end)


    t_it("https two-way valify local server test", function()
        t_wait(10000)

        local ctx = "hello"

        local port = 8184

        local srvConn = HttpConn()
        srvConn:listen_s(local_host, port, vfy_srv_ssl, nil,
            function(conn, http_type, code, method, url, body)
                t_equal(http_type, 0)

                -- 1 = GET, 3 = POST
                if "/get" == url then
                    t_equal(method, 1)
                else
                    t_equal(url, "/post")
                    t_equal(method, 3)
                end

                conn:send_pkt(string.format(HTTP.P200, ctx:len(), ctx))
            end)

        local cltConn = HttpConn()
        cltConn:connect_s(local_host, port, vfy_clt_ssl, function(_, ecode)
            t_equal(ecode, 0)

            cltConn:get("/get", nil,
                function(_, http_type, code, method, url, body)
                    local header = cltConn:get_header()
                    t_equal(code, 200)
                    t_equal(header.Server, "Mini-Game-Distribute-Server/1.0")

                    cltConn:post("/post", nil,
                        function(_, http_type2, code2, method2, url2, body2)
                            t_equal(code2, 200)
                            cltConn:close()
                            t_done()
                        end)
                end)
        end)
    end)
end)
