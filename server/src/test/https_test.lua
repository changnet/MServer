-- http(s)_test.lua
-- 2017-12-11
-- xzc

local util = require "engine.util"
local TlsCtx = require "engine.TlsCtx"
local HttpConn = require "http.http_conn"


t_describe("http(s) test", function()
    -- 产生一个缓存，避免下面连接时查询dns导致测试超时
    -- example.com不稳定，经常连不上，用postman来测试
    -- local exp_host = "www.example.com"
    local exp_host = "postman-echo.com"
    local exp_ip

    local local_host = IPV4 and "127.0.0.1" or "::1"

    local clt_ssl = TlsCtx()
    local srv_ssl = TlsCtx()
    local vfy_ssl = TlsCtx()
    local vfy_srv_ssl = TlsCtx()
    local vfy_clt_ssl = TlsCtx()

    t_before(function()
        clt_ssl:init() -- 客户端的ssl不需要证书

        if LINUX then
            vfy_ssl:init(nil, nil, nil, "/etc/ssl/certs/ca-certificates.crt")
        end

        srv_ssl:init("../certs/server.cer",
            "../certs/srv_key.pem", "mini_distributed_game_server")

        vfy_srv_ssl:init("../certs/server.cer",
            "../certs/srv_key.pem",
            "mini_distributed_game_server",
            "../certs/ca.cer")

        vfy_clt_ssl:init("../certs/client.cer",
            "../certs/clt_key.pem",
            "mini_distributed_game_server",
            "../certs/ca.cer")

        exp_ip = util.get_addr_info(exp_host)
        t_print("target host address is", exp_ip)
    end)

    t_it("http get " .. exp_host, function()
        t_async(15000)

        local conn = HttpConn()

        conn:connect(exp_host, 80, exp_ip)
        conn.on_connected = function(_conn)
            conn:get("/get", nil,
                     function(__conn, http_type, code, method, url, body)
                t_equal(http_type, 2)
                t_equal(code, 200)
                conn:close()
                t_done()
            end)
        end
    end)

    t_it("http post " .. exp_host, function()
        t_async(15000)

        local conn = HttpConn()

        conn:connect(exp_host, 80, exp_ip)
        conn.on_connected = function(_conn)
            conn:post("/post", nil,
                      function(__conn, http_type, code, method, url, body)
                t_equal(http_type, 2)
                t_equal(code, 200)
                conn:close()
                t_done()
            end)
        end
    end)

    t_it("http local server test", function()
        t_async(10000)

        local ctx = "hello"

        local port = 8182
        local no_len_ctx = "no_len_test ctx 1111111122222223333aaaabbbbbbccccc"

        local listen_conn = HttpConn()
        t_assert(listen_conn:listen(local_host, port))
        listen_conn.on_cmd = function(conn, http_type, code, method, url, body)
            t_equal(http_type, 1)
            t_equal(conn:address(), local_host)

            -- method 1 = GET, 3 = POST
            if "/get" == url then
                t_equal(method, 1)
                conn:send_pkt(string.format(HTTP.P200, ctx:len(), ctx))
            elseif "/post" == url then
                t_equal(method, 3)
                conn:send_pkt(string.format(HTTP.P200, ctx:len(), ctx))
            elseif "/nolength" == url then
                t_equal(method, 1)
                -- 测试http的一个特殊实现，当内容里不存在content-len时
                -- 数据长度以连接关闭时为准
                local no_len_p200 = 'HTTP/1.1 200 OK\r\n\z
                    Content-Type: application/json; charset=UTF-8\r\n\z
                    Server: Mini-Game-Distribute-Server/1.0\r\n\z
                    Connection: close\r\n\r\n' .. no_len_ctx
                conn:send_pkt(no_len_p200)
                conn:close(true)
            else
                t_assert(false)
            end
        end
        listen_conn.on_disconnected = function(conn, e)
            if 0 ~= e then
                print("error >>>>>>>>>>>>>>>>>>>", e)
            end
        end

        local clt_conn = HttpConn()
        t_assert(clt_conn:connect(local_host, port))
        clt_conn.on_connected = function(_)
            clt_conn:get("/get", nil,
                        function(_, http_type, code, method, url, body)
                local header = clt_conn:get_header()
                t_equal(code, 200)
                t_equal(header.Server, "Mini-Game-Distribute-Server/1.0")

                clt_conn:post("/post", nil,
                             function(_, http_type2, code2, method2, url2, body2)
                    t_equal(code2, 200)
                    clt_conn:get("/nolength", nil,
                        function(_, http_type3, code3, method3, url3, body3)
                            t_equal(code3, 200)
                            t_equal(body3, no_len_ctx)
                            clt_conn:close()
                            listen_conn:close()
                            t_done()
                        end)
                end)
            end)
        end
        clt_conn.on_disconnected = function(conn, e)
            if 0 ~= e then
                print("error >>>>>>>>>>>>>>>>>>>", e)
            end
        end
    end)

    t_it("https_get " .. exp_host, function()
        t_async(15000)

        local conn = HttpConn()

        conn:connect_s(exp_host, 443, clt_ssl)
        conn.on_connected = function(_conn)
            conn:get("/get", nil,
                     function(__conn, http_type, code, method, url, body)
                t_equal(http_type, 2)
                t_equal(code, 200)
                conn:close()
                t_done()
            end)
        end
    end)

    t_it("https post " .. exp_host, function()
        t_async(15000)

        local conn = HttpConn()

        conn:connect_s(exp_host, 443, clt_ssl)
        conn.on_connected = function(_conn)
            conn:post("/post", nil,
                      function(__conn, http_type, code, method, url, body)
                t_equal(http_type, 2)
                t_equal(code, 200)
                conn:close()
                t_done()
            end)
        end
    end)

    t_it("https local server test", function()
        t_async(10000)

        local ctx = "hello"

        local port = 8183

        local srv_conn = nil
        local listen_conn = HttpConn()
        listen_conn:listen_s(local_host, port, srv_ssl)
        listen_conn.on_cmd = function(conn, http_type, code, method, url, body)
            t_equal(http_type, 1)
            srv_conn = conn

            -- 1 = GET, 3 = POST
            if "/get" == url then
                t_equal(method, 1)
            else
                t_equal(url, "/post")
                t_equal(method, 3)
            end

            conn:send_pkt(string.format(HTTP.P200, ctx:len(), ctx))
        end

        local clt_conn = HttpConn()
        clt_conn:connect_s(local_host, port, clt_ssl)
        clt_conn.on_connected = function(_)
            clt_conn:get("/get", nil,
                        function(_, http_type, code, method, url, body)
                local header = clt_conn:get_header()
                t_equal(code, 200)
                t_equal(header.Server, "Mini-Game-Distribute-Server/1.0")

                clt_conn:post("/post", nil,
                             function(_, http_type2, code2, method2, url2, body2)
                    t_equal(code2, 200)
                    clt_conn:close()
                    srv_conn:close()
                    listen_conn:close()
                    t_done()
                end)
            end)
        end
    end)

    t_it("https ssl verify and get " .. exp_host, vfy_ssl, function()
        t_async(10000)

        local conn = HttpConn()

        conn:connect_s(exp_host, 443, vfy_ssl)
        conn.on_connected = function(_conn)
            conn:get("/get", nil,
                     function(__conn, http_type, code, method, url, body)
                t_equal(http_type, 2)
                t_equal(code, 200)
                conn:close()
                t_done()
            end)
        end
    end)

    t_it("https two-way ssl verify local server test", function()
        t_async(10000)

        local ctx = "hello"

        local port = 8184

        local srv_conn = nil
        local listen_conn = HttpConn()
        listen_conn:listen_s(local_host, port, vfy_srv_ssl)
        listen_conn.on_cmd = function(conn, http_type, code, method, url, body)
            t_equal(http_type, 1)
            srv_conn = conn

            -- 1 = GET, 3 = POST
            if "/get" == url then
                t_equal(method, 1)
            else
                t_equal(url, "/post")
                t_equal(method, 3)
            end

            conn:send_pkt(string.format(HTTP.P200, ctx:len(), ctx))
        end

        local clt_conn = HttpConn()
        clt_conn:connect_s(local_host, port, vfy_clt_ssl)
        clt_conn.on_connected = function(_)
            clt_conn:get("/get", nil,
                        function(_, http_type, code, method, url, body)
                local header = clt_conn:get_header()
                t_equal(code, 200)
                t_equal(header.Server, "Mini-Game-Distribute-Server/1.0")

                clt_conn:post("/post", nil,
                             function(_, http_type2, code2, method2, url2, body2)
                    t_equal(code2, 200)
                    clt_conn:close()
                    srv_conn:close()
                    listen_conn:close()
                    t_done()
                end)
            end)
        end
    end)
end)
