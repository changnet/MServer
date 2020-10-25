-- http(s)_test.lua
-- 2017-12-11
-- xzc

local network_mgr = network_mgr
local HttpConn = require "http.http_conn"
g_conn_mgr = require "network.conn_mgr"

t_describe("http(s) test", function()
    -- 产生一个缓存，避免下面连接时查询dns导致测试超时
    local host = "www.example.com"
    util.gethostbyname(host)

    network_mgr:new_ssl_ctx(network_mgr.SSLVT_TLS_CLT_AT)

    network_mgr:new_ssl_ctx(
        network_mgr.SSLVT_TLS_SRV_AT, "../certs/server.cer",
        "../certs/srv_key.pem","mini_distributed_game_server" )

    t_it("http get " .. host, function()
        t_wait(5000)

        local conn = HttpConn()

        conn:connect(host, 80, function(_conn, ecode)
            t_equal(ecode, 0)

            conn:get("/", nil,
                function(__conn, http_type, code, method, url, body)
                    t_equal(http_type, 1)
                    t_equal(code, 200)
                    conn:close()
                    t_done()
                end)
        end)
    end)

    t_it("http post " .. host, function()
        t_wait(5000)

        local conn = HttpConn()

        conn:connect(host, 80, function(_conn, ecode)
            t_equal(ecode, 0)

            conn:post("/", nil,
                function(__conn, http_type, code, method, url, body)
                    t_equal(http_type, 1)
                    t_equal(code, 200)
                    conn:close()
                    t_done()
                end)
        end)
    end)

    t_it("http local server test", function()
        t_wait(5000)

        local ctx = "hello"

        local port = 8182
        local host = "127.0.0.1"

        local srvConn = HttpConn()
        srvConn:listen(host, port, nil,
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
        cltConn:connect(host, port, function(_, ecode)
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


    t_it("https get " .. host, function()
        t_wait(5000)

        local conn = HttpConn()

        conn:connect_s(host, 443, nil, function(_conn, ecode)
            t_equal(ecode, 0)

            conn:get("/", nil,
                function(__conn, http_type, code, method, url, body)
                    t_equal(http_type, 1)
                    t_equal(code, 200)
                    conn:close()
                    t_done()
                end)
        end)
    end)

    t_it("https post " .. host, function()
        t_wait(5000)

        local conn = HttpConn()

        conn:connect_s(host, 443, nil, function(_conn, ecode)
            t_equal(ecode, 0)

            conn:post("/", nil,
                function(__conn, http_type, code, method, url, body)
                    t_equal(http_type, 1)
                    t_equal(code, 200)
                    conn:close()
                    t_done()
                end)
        end)
    end)

    t_it("https local server test", function()
        t_wait(5000)

        local ctx = "hello"

        local port = 8183
        local host = "127.0.0.1"

        local srvConn = HttpConn()
        srvConn:listen_s(host, port, "../certs/server.cer", nil,
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
        cltConn:connect_s(host, port, nil, function(_, ecode)
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
