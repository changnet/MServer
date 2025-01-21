-- http_conn.lua
-- 2017-12-16
-- xzc


local EngineSocket = require "engine.Socket"
local HTTP = require "http.http_header"

local Socket = require "network.socket"

local PAGE_GET = HTTP.PGET
local PAGE_POST = HTTP.PPOST
local OPENED = SocketMgr.OPENED

-- http连接
local HttpSocket = oo.class(Socket)

HttpSocket.default_param = {
    pkt = EngineSocket.PT_HTTP, -- 打包解包类型
}

function HttpSocket:__init()
    return Socket.__init(self)
end

-- 收到消息
-- @param http_type enum http_parser_type 0请求request，1 返回respond
-- @param code 错误码，如404，仅response有用
-- @param method 1 = GET, 3 = POST，仅request有用，其他值参考C++的定义
-- @param url 请求的url，仅request有用
-- @param body 数据
function HttpSocket:on_message(http_type, code, method, url, body)
    -- http调用比较少，不太需要考虑效率，直接绑定回调函数即可
    -- 这里只是给出各个回调参数的注释
    -- return self:on_message(http_type, code, method, url, body)
end

function HttpSocket:on_disconnected()
    -- http的特殊操作: 无Content-len时以对方关闭连接收到的数据为准
    if OPENED == self.status then
        local rcode, http_type, code, method, url, body = self.s:unpack_on_closed()
        if SocketMgr.PC_DATA == rcode then
            self:on_message(http_type, code, method, url, body)
        end
    end
end

-- 发送数据包
function HttpSocket:send_pkt(pkt)
    return self.s:send_pkt(pkt)
end

-- 格式化HOST字段
function HttpSocket:fmt_host(host, port, ssl)
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
function HttpSocket:get(url, req, cb)
    if cb then self.on_message = cb end

    local host = self:fmt_host(self.host, self.port, self.ssl)
    return self:send_pkt(string.format(PAGE_GET, url or "/", req and "?" or "",
                                       req or "", host))
end

-- 发起post请求
-- @param url 请求的url
-- @param body 请求的数据
-- @param cb 回调函数，不需要或者在connnect时已指定可为nil
function HttpSocket:post(url, body, cb)
    if cb then self.on_message = cb end

    body = body or ""
    local host = self:fmt_host(self.host, self.port, self.ssl)
    return self:send_pkt(string.format(PAGE_POST, url, host,
                                       string.len(body), body))
end

-- 获取http头信息(code仅在返回时有用，method仅在请求时有用)
-- @return upgrade, code, method, fields
function HttpSocket:get_header()
    return self.s:get_http_header()
end

return HttpSocket
