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
    pkt = network_mgr.PT_HTTP, -- 打包解包类型
}

function HttpConn:__init()
    Conn:__int(self)
end

-- 收到消息
-- @param http_type enum http_parser_type 0请求request，1 返回respond
-- @param code 错误码，如404，仅response有用
-- @param method 1 = GET, 3 = POST，仅request有用，其他值参考C++的定义
-- @param url 请求的url，仅request有用
-- @param body 数据
function HttpConn:on_cmd(http_type, code, method, url, body)
    -- http调用比较少，不太需要考虑效率，直接绑定回调函数即可
    -- 这里只是给出各个回调参数的注释
    -- return self:on_cmd(http_type, code, method, url, body)
end

-- 发送数据包
function HttpConn:send_pkt(pkt)
    return network_mgr:send_raw_packet(self.conn_id, pkt)
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
