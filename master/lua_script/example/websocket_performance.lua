-- websocket_performance.lua
-- 2017-11-29
-- xzc

-- https://tools.ietf.org/pdf/rfc6455.pdf section1.3 page6

-- example at: https://www.websocket.org/aboutwebsocket.html

local handshake_clt = table.concat(
{
    'GET ws://echo.websocket.org/?encoding=text HTTP/1.1\r\n',
    'Origin: http://websocket.org\r\n',
    'Cookie: __utma=99as\r\n',
    'Connection: Upgrade\r\n',
    'Host: echo.websocket.org\r\n',
    'Sec-WebSocket-Key: uRovscZjNol/umbTt5uKmw==\r\n',
    'Upgrade: websocket\r\n',
    'Sec-WebSocket-Version: 13\r\n\r\n',
} )

local handshake_srv =table.concat(
{    
    'HTTP/1.1 101 WebSocket Protocol Handshake\r\n',
    'Date: Fri, 10 Feb 2012 17:38:18 GMT\r\n',
    'Connection: Upgrade\r\n',
    'Server: Kaazing Gateway\r\n',
    'Upgrade: WebSocket\r\n',
    'Access-Control-Allow-Origin: http://websocket.org\r\n',
    'Access-Control-Allow-Credentials: true\r\n',
    'Sec-WebSocket-Accept: rLHCkw/SKsO9GAH/ZSFhBATDKrU=\r\n',
    'Access-Control-Allow-Headers: content-type\r\n\r\n',
} )

local ws_magic = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

-- websocket opcodes
local WS_OP_CONTINUE = 0x0
local WS_OP_TEXT     = 0x1
local WS_OP_BINARY   = 0x2
local WS_OP_CLOSE    = 0x8
local WS_OP_PING     = 0x9
local WS_OP_PONG     = 0xA

-- websocket marks
local WS_FINAL_FRAME = 0x10
local WS_HAS_MASK    = 0x20

local util = require "util"
local network_mgr = network_mgr

function ws_handshake_new( conn_id,sec_websocket_key,sec_websocket_accept )
    print( "ws_handshake_new",conn_id,sec_websocket_key,sec_websocket_accept )
    -- 服务器收到客户端的握手请求
    if sec_websocket_key then
        local sha1 = util.sha1_raw( sec_websocket_key,ws_magic )
        local base64 = util.base64( sha1 )

        print( base64 ) -- just test if match
        network_mgr:send_raw_packet( conn_id,handshake_srv )

        return
    end

    -- 客户端收到服务端的握手请求
    if sec_websocket_accept then
        -- TODO:验证sec_websocket_accept是否正确
        local ctx = "hello,websocket.I am Mini-Game-Distribute-Server"
        network_mgr:send_srv_packet( 
            conn_id,WS_OP_TEXT | WS_FINAL_FRAME | WS_HAS_MASK,ctx )
        return
    end
end

function http_command_new( conn_id )
    print( "http_command_new",conn_id )
end

function webs_accept_new( conn_id )
    network_mgr:set_conn_io( conn_id,network_mgr.IOT_NONE )
    network_mgr:set_conn_codec( conn_id,network_mgr.CDC_PROTOBUF )
    network_mgr:set_conn_packet( conn_id,network_mgr.PKT_WEBSOCKET )
end

function webs_connect_new( conn_id,ecode )
    network_mgr:set_conn_io( conn_id,network_mgr.IOT_NONE )
    network_mgr:set_conn_codec( conn_id,network_mgr.CDC_PROTOBUF )
    network_mgr:set_conn_packet( conn_id,network_mgr.PKT_WEBSOCKET )

    print( "webs_connect_new",conn_id,ecode,util.what_error( ecode ) )

    network_mgr:send_srv_packet( conn_id,handshake_clt )
end

function webs_connect_del( conn_id )
    print( "webs_connect_del",conn_id )
end

function webs_command_new( conn_id,ctx )
    print("webs_command_new",conn_id,ctx)
end

local ws_port = 10004
local ws_listen = network_mgr:listen( "127.0.0.1",ws_port,network_mgr.CNT_WEBS )
print( "websocket listen at port:",ws_port )

-- 用官方的服务器测试作为client是否正常
-- local ws_url = "echo.websocket.org"
-- local ip1,ip2 = util.gethostbyname( ws_url )
-- local ws_conn = network_mgr:connect( ip1,80,network_mgr.CNT_WEBS )
-- print( "websocket connect to :",ws_url,ip1,"conn_id = ",ws_conn )

-- 测试自己的服务器是否正常
local ws_local_conn = 
    network_mgr:connect( "127.0.0.1",ws_port,network_mgr.CNT_WEBS )
print( "websocket connect to local server:",ws_port,"conn_id =",ws_local_conn )

local idx = network_mgr:new_ssl_ctx( 4,
    "certs/server.cer",2,"certs/srv_key.pem","mini_distributed_game_server" )
print( "create one ssl ctx at ",idx )
