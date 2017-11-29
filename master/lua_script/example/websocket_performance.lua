-- websocket_performance.lua
-- 2017-11-29
-- xzc

-- https://tools.ietf.org/pdf/rfc6455.pdf section1.3 page6
local handshake_clt =
[==[
GET /chat HTTP/1.1
Host: server.example.com
Upgrade: websocket
Connection: Upgrade
Sec-WebSocket-Key: %s
Origin: http://example.com
Sec-WebSocket-Protocol: chat, superchat
Sec-WebSocket-Version: 13
]==]

local handshake_srv =
[==[
HTTP/1.1 101 Switching Protocols
Upgrade: websocket
Connection: Upgrade
Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=
]==]

local sec_key  = "dGhlIHNhbXBsZSBub25jZQ=="
local ws_magic = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

local util = require "util"
local network_mgr = network_mgr

local ws_port = 10004
local ws_listen = network_mgr:listen( "0,0,0,0",ws_port,network_mgr.CNT_WEBS )
print( "websocket listen at port:",ws_port )

function ws_handshake_new( conn_id,sec_websocket_key,sec_websocket_accept )
    -- 服务器收到客户端的握手请求
    if sec_websocket_key then
        local sha1 = util.sha1( sec_websocket_key,ws_magic )
        local base64 = util.base64( sha1 )
        local ctx = string.format( handshake_srv,base64 )
        network_mgr:send_http_packet( conn_id,ctx )

        return
    end

    -- 客户端收到服务端的握手请求
    if sec_websocket_accept then
        -- TODO:验证sec_websocket_accept是否正确
        return
    end
end

function webs_accept_new( conn_id )
    network_mgr:set_conn_io( conn_id,network_mgr.IOT_NONE )
    network_mgr:set_conn_codec( conn_id,network_mgr.CDC_PROTOBUF )
    network_mgr:set_conn_packet( conn_id,network_mgr.PKT_WEBSOCKET )
end

function webs_connect_new( conn_id )
end

function webs_connect_del( conn_id )
end
