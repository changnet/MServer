-- websocket_performance.lua
-- 2017-11-29
-- xzc

-- https://tools.ietf.org/pdf/rfc6455.pdf section1.3 page6

-- example at: https://www.websocket.org/aboutwebsocket.html

local conn_mgr = require "network.conn_mgr"

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

local Clt_conn = oo.class( nil,"Clt_conn" )

function Clt_conn:handshake_new( sec_websocket_key,sec_websocket_accept )
    print( "clt handshake",sec_websocket_accept)
    if not sec_websocket_accept then return end

    -- TODO:验证sec_websocket_accept是否正确
    local ctx = "hello,websocket.I am Mini-Game-Distribute-Server"
    network_mgr:send_srv_packet( 
        self.conn_id,WS_OP_TEXT | WS_FINAL_FRAME | WS_HAS_MASK,ctx )
end

function Clt_conn:connect( ip,port )
    self.conn_id = network_mgr:connect( ip,port,network_mgr.CNT_CSCN )
    conn_mgr:set_conn( self.conn_id,self )
    print( "clt connnect to ",ip,self.conn_id )
end

function Clt_conn:conn_new( ecode )
    if 0 ~= ecode then
        print( "clt_conn conn error",self.conn_id,util.what_error(ecode) )
        return
    end

    network_mgr:set_conn_io( self.conn_id,network_mgr.IOT_NONE )
    network_mgr:set_conn_codec( self.conn_id,network_mgr.CDC_NONE )
    network_mgr:set_conn_packet( self.conn_id,network_mgr.PKT_WEBSOCKET )

    print( "conn_new",self.conn_id )

    network_mgr:send_srv_packet( self.conn_id,handshake_clt )
end

function Clt_conn:conn_del()
    print("conn_del",self.conn_id)
end

function Clt_conn:command_new( body )
    print("clt command new",body)
end

local Srv_conn = oo.class( nil,"Srv_conn" )

function Srv_conn:__init( conn_id )
    self.conn_id = conn_id
end

function Srv_conn:handshake_new( sec_websocket_key,sec_websocket_accept )
    print( "srv handshake_new",sec_websocket_key )
    -- 服务器收到客户端的握手请求
    if not sec_websocket_key then return end

    local sha1 = util.sha1_raw( sec_websocket_key,ws_magic )
    local base64 = util.base64( sha1 )

    print( base64 ) -- just test if match
    network_mgr:send_raw_packet( self.conn_id,handshake_srv )
end

function Srv_conn:listen( ip,port )
    self.conn_id = network_mgr:listen( ip,port,network_mgr.CNT_SCCN )
    conn_mgr:set_conn( self.conn_id,self )
    PLOG( "listen at %s:%d",ip,port )
end

function Srv_conn:conn_accept( new_conn_id )
    print( "srv conn accept new",new_conn_id )

    local new_conn = Srv_conn( new_conn_id )
    network_mgr:set_conn_io( new_conn_id,network_mgr.IOT_NONE )
    network_mgr:set_conn_codec( new_conn_id,network_mgr.CDC_NONE )
    network_mgr:set_conn_packet( new_conn_id,network_mgr.PKT_WEBSOCKET )

    return new_conn
end

function Srv_conn:conn_del()
    print( "srv conn del",self.conn_id )
end

-- 回调
function Srv_conn:command_new( body )
    print( "srv command_new",self.conn_id,body )

    local tips = "Mini-Game-Distribute-Server!"

    network_mgr:send_clt_packet( self.conn_id,WS_OP_TEXT | WS_FINAL_FRAME,tips )
end

-- 用官方的服务器测试作为client是否正常
local ws_url = "echo.websocket.org"
local ip1,ip2 = util.gethostbyname( ws_url )

-- 这里创建的对象要放到全局引用，不然会被释放掉，就没法回调了

ws_conn = Clt_conn()
ws_conn:connect( ip1,80 )

-- 开户本地服务器
local ws_port = 10002
ws_listen = Srv_conn()
ws_listen:listen( "127.0.0.1",ws_port )

-- 测试自己的服务器是否正常
ws_local_conn = Clt_conn()
ws_local_conn:connect( "127.0.0.1",ws_port )


