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

HttpConn.default_param = {
    cdt = network_mgr.CDT_NONE, -- 编码类型
    pkt = network_mgr.PT_HTTP, -- 打包类型
}

function HttpConn:__init(conn_id)
    self.conn_id = conn_id -- 通过accept创建时需要直接指定
end

function HttpConn:conn_del()
end

function HttpConn:conn_ok()
    if self.on_connected then return self.on_connected(self, 0) end
    if self.on_created then self.on_created(self) end
end

-- 收到消息
-- @param http_type enum http_parser_type 0请求request，1 返回respond
-- @param code 错误码，如404，仅response有用
-- @param method 1 = GET, 3 = POST，仅request有用，其他值参考C++的定义
-- @param url 请求的url，仅request有用
-- @param body 数据
function HttpConn:command_new(http_type, code, method, url, body)
    return self:on_cmd(http_type, code, method, url, body)
end

-- 发送数据包
function HttpConn:send_pkt(pkt)
    return network_mgr:send_raw_packet(self.conn_id, pkt)
end

-- 连接到其他服务器
-- @param host 目标服务器地址
-- @param port 目标服务器端口
-- @param on_connected 连接成功(或失败)时的回调函数
-- @param on_cmd 收到请求时回调函数，不需要可为nil
function HttpConn:connect(host, port, on_connected, on_cmd)
    self.ip = util.get_addr_info(host)
    -- 这个host需要注意，实测对www.example.com请求时，如果host为一个ip，是会返回404的
    self.host = host
    self.port = port
    self.on_connected = on_connected
    self.on_cmd = on_cmd

    self.conn_id = network_mgr:connect(self.ip, port, network_mgr.CT_CSCN)

    self:set_conn(self.conn_id, self)
end

-- 以https试连接到其他服务器
-- @param host 目标服务器地址
-- @param port 目标服务器端口
-- @param ssl 用new_ssl_ctx创建的ssl_ctx
-- @param on_connected 连接成功(或失败)时的回调函数
-- @param on_cmd 收到请求时回调函数，不需要可为nil
function HttpConn:connect_s(host, port, ssl, on_connected, on_cmd)
    self.ssl = assert(ssl)

    return self:connect(host, port, on_connected, on_cmd)
end

-- 关闭链接
-- @param flush 关闭前是否发送缓冲区的数据
function HttpConn:close(flush)
    self:set_conn(self.conn_id, nil)
    return network_mgr:close(self.conn_id, flush)
end

-- 监听http连接
-- @param on_cmd 收到请求时的回调函数，可为nil
-- @param on_created 接受新连接时的回调函数，可为nil
function HttpConn:listen(ip, port, on_created, on_cmd)
    self.on_created = on_created
    self.on_cmd = on_cmd
    self.conn_id = network_mgr:listen(ip, port, network_mgr.CT_SCCN)

    self:set_conn(self.conn_id, self)
    return true
end

-- 以https方式监听http连接
-- @param ssl 用new_ssl_ctx创建的ssl_ctx
-- @param on_cmd 收到请求时的回调函数，可为nil
-- @param on_created 接受新连接时的回调函数，可为nil
function HttpConn:listen_s(ip, port, ssl, on_created, on_cmd)
    self.ssl = assert(ssl)
    return self:listen(ip, port, on_created, on_cmd)
end

-- 连接成功(或失败)
function HttpConn:conn_new(ecode)
    if 0 == ecode then
        self:set_conn_param()
    else
        if self.on_connected then return self.on_connected(self, ecode) end
    end
end

-- 格式化HOST字段
function HttpConn:fmt_host(host, port, ssl)
    if port == 80 and not ssl then
        return host
    end

    if port == 443 and ssl then
        return host
    end

    return string.format("%s:%d", host, port)
end

-- 发起get请求
-- @param url 请求的url
-- @param req 请求的数据，不包含url，已进行url转义。如: name=1&pass=2
-- @param cb 回调函数，不需要或者在connnect时已指定可为nil
function HttpConn:get(url, req, cb)
    if cb then self.on_cmd = cb end

    local host = self:fmt_host(self.host, self.port, self.ssl)
    return self:send_pkt(string.format(PAGE_GET, url or "/", req and "?" or "",
                                       req or "", host))
end

-- 发起post请求
-- @param url 请求的url
-- @param body 请求的数据
-- @param cb 回调函数，不需要或者在connnect时已指定可为nil
function HttpConn:post(url, body, cb)
    if cb then self.on_cmd = cb end

    body = body or ""
    local host = self:fmt_host(self.host, self.port, self.ssl)
    return self:send_pkt(string.format(PAGE_POST, url, host,
                                       string.len(body), body))
end

-- 获取http头信息(code仅在返回时有用，method仅在请求时有用)
-- return upgrade, code, method, fields
function HttpConn:get_header()
    return network_mgr:get_http_header(self.conn_id)
end

return HttpConn
