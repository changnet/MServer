-- 客户端网络连接


local handshake_srv =table.concat(
{
    'HTTP/1.1 101 WebSocket Protocol Handshake\r\n',
    'Connection: Upgrade\r\n',
    'Upgrade: WebSocket\r\n',
    'Sec-WebSocket-Accept: %s\r\n\r\n',
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
local g_command_mgr = g_command_mgr

local CltConn = oo.class( ... )

function CltConn:__init( conn_id )
    self.auth = false
    self.beat = 0
    self.fchk = 0 -- fail check

    self.conn_id = conn_id
end

function CltConn:handshake_new( sec_websocket_key,sec_websocket_accept )
    -- 服务器收到客户端的握手请求
    if not sec_websocket_key then
        self.close()
        PRINTF( "clt handshake no sec_websocket_key")
        return
    end

    local sha1 = util.sha1_raw( sec_websocket_key,ws_magic )
    local base64 = util.base64( sha1 )

    PRINTF("clt handshake %d",self.conn_id)
    return network_mgr:send_raw_packet(
        self.conn_id,string.format(handshake_srv,base64) )
end

-- 发送数据包
function CltConn:send_pkt( cmd,pkt,errno )
    -- 使用tcp二进制流
    return network_mgr:send_clt_packet( self.conn_id,cmd.i,errno or 0,pkt )
    -- 使用websocket
    -- return network_mgr:send_clt_packet(
    --     self.conn_id,cmd,errno or 0,WS_OP_BINARY | WS_FINAL_FRAME,pkt )
end

-- 认证成功
function CltConn:authorized()
    self.auth = true
end

-- 将该链接绑定一个角色
function CltConn:bind_role( pid )
    self.pid = pid
    network_mgr:set_conn_owner( self.conn_id,self.pid or 0 )
end

-- 监听客户端连接
function CltConn:listen( ip,port )
    self.conn_id = network_mgr:listen( ip,port,network_mgr.CNT_SCCN )

    g_conn_mgr:set_conn( self.conn_id,self )
end

-- 连接断开
function CltConn:conn_del()
    -- 这个会自动解除
    -- g_conn_mgr:set_conn( self.conn_id,nil )
    network_mgr:unset_conn_owner( self.conn_id,self.pid or 0 )

    return g_network_mgr:clt_conn_del( self.conn_id )
end

-- 消息回调
function CltConn:command_new( cmd,... )
    return g_command_mgr:clt_dispatch( self,cmd,... )
end

function CltConn:ctrl_new( flag,body )
    -- 控制帧只在前4位，先去掉WS_HAS_MASK
    flag = flag & 0x0F
    if flag == WS_OP_CLOSE then
        return network_mgr:send_ctrl_packet(
            self.conn_id,WS_OP_CLOSE | WS_FINAL_FRAME )
        -- 服务器通常不主动关闭连接，这理暂时不处理close
    elseif flag == WS_OP_PING then
        -- 返回pong时，如果对方ping时发了body，一定要原封不动返回
        return network_mgr:send_ctrl_packet(
            self.conn_id,WS_OP_PONG | WS_FINAL_FRAME,body )
    elseif flag == WS_OP_PONG then
        -- TODO: 这里更新心跳
        return
    end

    assert( false,"unknow ctrl flag" )
end

-- 主动关闭连接
function CltConn:close()
    g_conn_mgr:set_conn( self.conn_id,nil )
    network_mgr:unset_conn_owner( self.conn_id,self.pid or 0 )

    return network_mgr:close( self.conn_id )
end

-- 接受新客户端连接
function CltConn:conn_accept( new_conn_id )
    network_mgr:set_conn_io( new_conn_id,network_mgr.IOT_NONE )
    network_mgr:set_conn_codec( new_conn_id,network_mgr.CDC_PROTOBUF )

    -- 使用tcp二进制流
    network_mgr:set_conn_packet( new_conn_id,network_mgr.PKT_STREAM )
    -- 使用websocket二进制流
    -- network_mgr:set_conn_packet( new_conn_id,network_mgr.PKT_WSSTREAM )

    -- set_send_buffer_size最后一个参数表示over_action，1 = 溢出后断开
    network_mgr:set_send_buffer_size( new_conn_id,128,8192,1 ) -- 8k*128 = 1024k
    network_mgr:set_recv_buffer_size( new_conn_id,8,8192 ) -- 8k*8 = 64k

    local new_conn = CltConn( new_conn_id )
    g_network_mgr:clt_conn_accept( new_conn_id,new_conn )

    return new_conn
end

return CltConn
