-- http deamon，一个简单的http服务器，专门处理www目录下的内容，通常用于gm以及后台通信
local Httpd = oo.singleton(...)

local uri = require "util.uri"
local HTTP = require "http.http_header"
local HttpConn = require "http.http_conn"

local page404 = HTTP.P404
local page500 = HTTP.P500
local page200 = HTTP.P200_CLOSE

function Httpd:__init()
    self.conn = {}
    self.exec = {}
    self.listen_conn = nil
end

-- 收到新连接时放到列表
function Httpd:on_accepted(conn)
    -- 定时踢出不断开的连接
    -- TODO 算了，这个是内部使用，暂时不做超时处理
    print("httpd accept connection", conn.conn_id)
end

-- 启动http服务器
function Httpd:start(ip, port)
    assert(not self.listen_conn)

    local listen_conn = HttpConn()

    -- 用函数wrap一层，这样不影响热更
    listen_conn:listen(ip, port)
    listen_conn.on_accepted = function(conn)
        return self:on_accepted(conn)
    end

    listen_conn.on_cmd = function(conn, http_type, code, method, url, body)
        return self:do_command(conn, http_type, code, method, url, body)
    end

    printf("httpd listen at %s:%d", ip, port)

    self.listen_conn = listen_conn
    return true;
end

-- 停止http服务器
function Httpd:stop()
    self.listen_conn:close()
    self.listen_conn = nil

    -- TODO 关闭self.conn中的所有连接
    print("httpd stop")
end

-- 处理http请求
function Httpd:do_exec(path, conn, fields, body)
    local exec_obj = require(path)
    return exec_obj:exec(conn, fields, body)
end

-- 格式化错误码为json格式
function Httpd:format_error(code, ctx)
    if not ctx then
        return string.format('{"code":%d,"msg":"%s"}', code[1], code[2])
    end

    if code[2] then
        return string.format('{"code":%d,"msg":"%s","ctx":"%s"}', code[1],
                             code[2], ctx)
    end

    -- 直接返回由对应功能指定的内容。比如返回一个json串，不好再放到ctx里
    return ctx
end

-- 格式化http-200返回
function Httpd:format_200(code, ctx)
    local error_ctx = self:format_error(code, ctx)

    return string.format(page200, string.len(error_ctx), error_ctx)
end

-- 处理返回
function Httpd:do_return(conn, success, code, ctx)
    if not success then -- 发生语法错误
        conn:send_pkt(page500)
    else
        -- 需要异步处理数据，阻塞中
        -- TODO:要不要加个定时器做超时?
        if code == HTTP.PENDING then return end

        conn:send_pkt(self:format_200(code, ctx))
    end

    -- return conn:close(true)
end

local httpd = Httpd()

-- http回调
function Httpd:do_command(conn, http_type, code, method, url, body)
    -- url = /platform/pay?sid=99&money=200
    print("http", url, body)
    local raw_url, fields = uri.parse(url)
print(raw_url)
vd(fields)
    local path = self.exec[raw_url]
    if not path then
        -- 限定http请求的路径，不能随意运行其他路径文件
        -- 也不要随意放其他文件到此路径
        path = "http/www" .. raw_url
        local exec_file = io.open("../src/" .. path .. ".lua", "r")

        if not exec_file then
            elog("http request page not found:%s", path)
            conn:send_pkt(page404)

            return conn:close(true)
        end

        io.close(exec_file)
        path = string.gsub(path, "%/", ".") -- 把/转为.来匹配lua的require格式
        -- 记录一个path而不是一个exec_obj，不影响热更，但不用每次都拼字符
        self.exec[raw_url] = path
    end

    local success, ecode, ctx = xpcall(Httpd.do_exec, __G__TRACKBACK, httpd,
                                       path, conn, fields, body)
print("check result >>>>>>>>>>>>", success, ecode, ctx)
    return self:do_return(conn, success, ecode, ctx)
end

return httpd
