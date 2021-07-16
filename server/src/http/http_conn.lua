-- http_conn.lua
-- 2017-12-16
-- xzc



local HTTP = require "http.http_header"

local Conn = require "network.conn"

local PAGE_GET = HTTP.PGET
local PAGE_POST = HTTP.PPOST

local network_mgr = network_mgr

-- http连接
local HttpConn = oo.class(..., Conn)

function HttpConn:__init(conn_id)
    self.conn_id = conn_id -- 通过accept创建时需要直接指定
end

function HttpConn:conn_del()
end

function HttpConn:conn_ok()
    if self.on_connect then return self.on_connect(self, 0) end
    if self.on_accept then self.on_accept(self) end
end

-- 收到消息
-- @param http_type enum http_parser_type 0请求request，1 返回respond
-- @param code 错误码，如404，仅response有用
-- @param method 1 = GET, 3 = POST，仅request有用，其他值参考C++的定义
-- @param url 请求的url，仅request有用
-- @param body 数据
function HttpConn:command_new(http_type, code, method, url, body)
    return self:on_command(http_type, code, method, url, body)
end

-- 发送数据包
function HttpConn:send_pkt(pkt)
    return network_mgr:send_raw_packet(self.conn_id, pkt)
end

-- 连接到其他服务器
-- @param host 目标服务器地址
-- @param port 目标服务器端口
-- @param on_connect 连接成功(或失败)时的回调函数
-- @param on_command 收到请求时回调函数，不需要可为nil
function HttpConn:connect(host, port, on_connect, on_command)
    self.ip = util.get_addr_info(host)
    -- 这个host需要注意，实测对www.example.com请求时，如果host为一个ip，是会返回404的
    self.host = host
    self.port = port
    self.on_connect = on_connect
    self.on_command = on_command

    self.conn_id = network_mgr:connect(self.ip, port, network_mgr.CT_CSCN)

    self:set_conn(self.conn_id, self)
end

-- 以https试连接到其他服务器
-- @param host 目标服务器地址
-- @param port 目标服务器端口
-- @param ssl 用new_ssl_ctx创建的ssl_ctx
-- @param on_connect 连接成功(或失败)时的回调函数
-- @param on_command 收到请求时回调函数，不需要可为nil
function HttpConn:connect_s(host, port, ssl, on_connect, on_command)
    self.ssl = assert(ssl)

    return self:connect(host, port, on_connect, on_command)
end

-- 关闭链接
-- @param flush 关闭前是否发送缓冲区的数据
function HttpConn:close(flush)
    self:set_conn(self.conn_id, nil)
    return network_mgr:close(self.conn_id, flush)
end

-- 监听http连接
-- @param on_command 收到请求时的回调函数，可为nil
-- @param on_accept 接受新连接时的回调函数，可为nil
function HttpConn:listen(ip, port, on_accept, on_command)
    self.on_accept = on_accept
    self.on_command = on_command
    self.conn_id = network_mgr:listen(ip, port, network_mgr.CT_SCCN)

    self:set_conn(self.conn_id, self)
    return true
end

-- 以https方式监听http连接
-- @param ssl 用new_ssl_ctx创建的ssl_ctx
-- @param on_command 收到请求时的回调函数，可为nil
-- @param on_accept 接受新连接时的回调函数，可为nil
function HttpConn:listen_s(ip, port, ssl, on_accept, on_command)
    self.ssl = assert(ssl)
    return self:listen(ip, port, on_accept, on_command)
end

-- 初始化连接参数
function HttpConn:init_conn(conn_id, ssl)
    if ssl then
        network_mgr:set_conn_io(conn_id, network_mgr.IOT_SSL, ssl)
    else
        network_mgr:set_conn_io(conn_id, network_mgr.IOT_NONE)
    end
    network_mgr:set_conn_codec(conn_id, network_mgr.CT_NONE)
    network_mgr:set_conn_packet(conn_id, network_mgr.PT_HTTP)
end

-- 有新的连接进来
function HttpConn:conn_accept(new_conn_id)
    local new_conn = HttpConn(new_conn_id)

    -- 继承一些参数
    new_conn.ssl = self.ssl
    new_conn.on_command = self.on_command
    new_conn:init_conn(new_conn_id, self.ssl)

    return new_conn
end

-- 连接成功(或失败)
function HttpConn:conn_new(ecode)
    if 0 == ecode then
        self:init_conn(self.conn_id, self.ssl)
    else
        if self.on_connect then return self.on_connect(self, ecode) end
    end
end

-- 发起get请求
-- @param url 请求的url
-- @param req 请求的数据，不包含url，已进行url转义。如: name=1&pass=2
-- @param cb 回调函数，不需要或者在connnect时已指定可为nil
function HttpConn:get(url, req, cb)
    if cb then self.on_command = cb end

    return self:send_pkt(string.format(PAGE_GET, url or "/", req and "?" or "",
                                       req or "", self.host, self.port))
end

-- 发起post请求
-- @param url 请求的url
-- @param body 请求的数据
-- @param cb 回调函数，不需要或者在connnect时已指定可为nil
function HttpConn:post(url, body, cb)
    if cb then self.on_command = cb end

    body = body or ""
    return self:send_pkt(string.format(PAGE_POST, url, self.host, self.port,
                                       string.len(body), body))
end

-- 获取http头信息(code仅在返回时有用，method仅在请求时有用)
-- return upgrade, code, method, fields
function HttpConn:get_header()
    return network_mgr:get_http_header(self.conn_id)
end

return HttpConn
