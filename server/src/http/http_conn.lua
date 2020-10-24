-- http_conn.lua
--- 2017-12-16
-- xzc

-- http连接

local HTTP = require "http.http_header"

local PAGE_GET = HTTP.PGET
local PAGE_POST = HTTP.PPOST

local network_mgr = network_mgr

local HttpConn = oo.class( ... )

function HttpConn:__init(conn_id)
    self.conn_id = conn_id -- 通过accept创建时需要直接指定
end

function HttpConn:conn_del()
end

function HttpConn:command_new( url,body )
    return self.on_command(self, url, body)
end

-- 发送数据包
function HttpConn:send_pkt( pkt )
    return network_mgr:send_raw_packet( self.conn_id,pkt )
end

-- 连接到其他服务器
-- @param host 目标服务器地址
-- @param port 目标服务器端口
-- @param on_connect 连接成功(或失败)时的回调函数
-- @param on_command 收到请求时回调函数，不需要可为nil
function HttpConn:connect( host, port, on_connect, on_command )
    self.ip = util.gethostbyname(host)
    -- 这个host需要注意，实测对www.example.com请求时，如果host为一个ip，是会返回404的
    self.host = host
    self.port = port
    self.on_connect = on_connect
    self.on_command = on_command

    self.conn_id = network_mgr:connect( self.ip,port,network_mgr.CNT_CSCN )

    g_conn_mgr:set_conn( self.conn_id,self )
end

-- 关闭链接
-- @param flush 关闭前是否发送缓冲区的数据
function HttpConn:close( flush )
    g_conn_mgr:set_conn( self.conn_id,nil )
    return network_mgr:close( self.conn_id,flush )
end

-- 监听http连接
-- @param on_command 收到请求时的回调函数
function HttpConn:listen( ip,port, on_command )
    self.on_command = on_command
    self.conn_id = network_mgr:listen( ip,port,network_mgr.CNT_SCCN )

    g_conn_mgr:set_conn( self.conn_id,self )
    return true
end

-- 有新的连接进来
function HttpConn:conn_accept( new_conn_id )
    network_mgr:set_conn_io( new_conn_id,network_mgr.IOT_NONE )
    network_mgr:set_conn_codec( new_conn_id,network_mgr.CDC_NONE )
    network_mgr:set_conn_packet( new_conn_id,network_mgr.PKT_HTTP )

    local new_conn = HttpConn( new_conn_id )

    new_conn.on_command = self.on_command
    return new_conn
end

-- 连接成功(或失败)
function HttpConn:conn_new( ecode )
    if 0 == ecode then
        network_mgr:set_conn_io( self.conn_id, network_mgr.IOT_NONE )
        network_mgr:set_conn_codec( self.conn_id,network_mgr.CDC_NONE )
        network_mgr:set_conn_packet( self.conn_id,network_mgr.PKT_HTTP )
    end

    if self.on_connect then return self.on_connect(self, ecode) end
end

-- 发起get请求
-- @param url 请求的url
-- @param req 请求的数据，不包含url，已进行url转义。如: name=1&pass=2
-- @param cb 回调函数，不需要或者在connnect时已指定可为nil
function HttpConn:get(url, req, cb)
    if cb then self.on_command = cb end

    return self:send_pkt(string.format(PAGE_GET,
        url or "/", req and "?" or "", req or "", self.host, self.port))
end

-- 发起post请求
-- @param url 请求的url
-- @param body 请求的数据
-- @param cb 回调函数，不需要或者在connnect时已指定可为nil
function HttpConn:post(url, body, cb)
    if cb then self.on_command = cb end

    body = body or ""
    return self:send_pkt(string.format(
        PAGE_POST, url, self.host, self.port, string.len(body), body))
end

-- 获取http头信息(code仅在返回时有用，method仅在请求时有用)
-- return upgrade, code, method, fields
function HttpConn:get_header()
    return network_mgr:get_http_header(self.conn_id)
end

return HttpConn
