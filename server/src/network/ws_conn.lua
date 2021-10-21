local util = require "engine.util"
local HttpConn = require "http.http_conn"

-- websocket公共层
local WsConn = oo.class(...)

local handshake_clt = 'GET %s HTTP/1.1\r\n\z
    Connection: Upgrade\r\n\z
    Host: %s\r\n\z
    Sec-WebSocket-Key: %s\r\n\z
    Upgrade: websocket\r\n\z
    Sec-WebSocket-Version: 13\r\n\r\n'

local handshake_srv = 'HTTP/1.1 101 WebSocket Protocol Handshake\r\n\z
    Connection: Upgrade\r\n\z
    Upgrade: WebSocket\r\n\z
    Sec-WebSocket-Accept: %s\r\n\r\n'

local ws_magic = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

local default_url = "/"

-- websocket opcodes
local WS_OP_CONTINUE = 0x0
local WS_OP_TEXT     = 0x1
local WS_OP_BINARY   = 0x2
local WS_OP_CLOSE = 0x8
local WS_OP_PING = 0x9
local WS_OP_PONG = 0xA

-- websocket marks
local WS_FINAL_FRAME = 0x10
local WS_HAS_MASK    = 0x20

local SRV_MASK = WS_FINAL_FRAME
local CLT_MASK = WS_FINAL_FRAME | WS_HAS_MASK

-- 导出一些常量

WsConn.WS_OP_CONTINUE = WS_OP_CONTINUE
WsConn.WS_OP_TEXT = WS_OP_TEXT
WsConn.WS_OP_BINARY = WS_OP_BINARY
WsConn.WS_OP_CLOSE = WS_OP_CLOSE
WsConn.WS_OP_PING = WS_OP_PING
WsConn.WS_OP_PONG = WS_OP_PONG
WsConn.WS_FINAL_FRAME = WS_FINAL_FRAME
WsConn.WS_HAS_MASK = WS_HAS_MASK


WsConn.default_param = {
    listen_type = network_mgr.CT_SCCN, -- 监听的连接类型
    connect_type = network_mgr.CT_CSCN, -- 连接类型
    iot = network_mgr.IOT_NONE, -- io类型
    cdt = network_mgr.CDT_PROTOBUF, -- 编码类型
    pkt = network_mgr.PT_WEBSOCKET, -- 打包类型
    action = 1, -- over_action，1 表示缓冲区溢出后断开
    chunk_size = 8192, -- 单个缓冲区大小
    send_chunk_max = 128, -- 发送缓冲区数量
    recv_chunk_max = 8 -- 接收缓冲区数
}

-- 处理websocket握手
function WsConn:handshake_new(sec_websocket_key, sec_websocket_accept)
    if sec_websocket_key then
        -- 服务器收到客户端的握手请求
        local sha1 = util.sha1_raw(sec_websocket_key, ws_magic)
        local base64 = util.base64(sha1)
        network_mgr:send_raw_packet(self.conn_id,
                                    string.format(handshake_srv, base64))
    else
        -- 客户端收到服务器返回的握手请求
        local sha1 = util.sha1_raw(self.ws_key, ws_magic)
        local base64 = util.base64(sha1)
        if sec_websocket_accept ~= base64 then
            print("websocket handshake key not match",
                self.conn_id, sec_websocket_accept, base64)
            self:close()
            return
        end
    end

    -- 握手后，连接建立成功
    self:on_connected()
end

-- 客户端发送握手请求
function WsConn:send_handshake(url)
    -- RFC 6455 4.1.7
    -- Sec-WebSocket-Key|. The value of this header field MUST be a
    -- nonce consisting of a randomly selected 16-byte value that has
    -- been base64-encoded
    -- 随机16个字节的字符串就可以了，主要是用于服务器返回时用于验证，确保对方是一个ws而
    -- 不是一个普通的http服务器
    local r_val = tostring(math.random(1000000000000001, 9999999999999999))
    self.ws_key = util.base64(r_val)

    local host = HttpConn:fmt_host(self.host, self.port, self.ssl)
    network_mgr:send_raw_packet(self.conn_id, string.format(
        handshake_clt, url or default_url, host, self.ws_key))
end

-- io建立成功，开始websocket握手
function WsConn:io_ok()
    -- 如果是客户端才发起握手，服务器是不处理的
    if not self.host then return end
    return self:send_handshake(self.url)
end

-- 发送原生数据包
-- @param body 要发送的文字
function WsConn:send_pkt(body)
    local mask = self.ws_key and CLT_MASK or SRV_MASK
    return network_mgr:send_clt_packet(self.conn_id, WS_OP_TEXT | mask, body)
end

-- 发送控制包
-- @param flag 控制包掩码，按位表示，如 WS_OP_PONG | WS_FINAL_FRAME
-- @param body 控制包的数据，可以为nil
function WsConn:send_ctrl(flag, body)
    local mask = self.ws_key and CLT_MASK or SRV_MASK
    return network_mgr:send_ctrl_packet(self.conn_id, flag | mask, body)
end

function WsConn:ws_close()
    -- 如果握手成功，则关闭时需要发送关闭数据包(用于监听的socket根本不会有握手)
    if not self.ok then return end

    self.op_close = true
    self:send_ctrl(WS_OP_CLOSE)
end

-- 处理控制包
function WsConn:ctrl_new(flag, body)
    -- 控制帧只在前4位，先去掉WS_HAS_MASK
    flag = flag & 0x0F
    if self.on_ctrl then self:on_ctrl(flag, body) end

    if flag == WS_OP_CLOSE then
        -- 自己发起关闭的，收到对方回复，可以直接关闭了
        if self.op_close then return end

        local mask = self.ws_key and CLT_MASK or SRV_MASK
        network_mgr:send_ctrl_packet(self.conn_id, WS_OP_CLOSE | mask)

        -- RFC6455 5.5.1
        -- If an endpoint receives a Close frame and did not previously send a
        -- Close frame, the endpoint MUST send a Close frame in response
        -- The server MUST close the underlying TCP connection immediately;
        -- 在chrome中，调用websocket的close函数后，如果服务器不主动关闭，可能会几分钟
        -- 后才会关闭连接
        -- https://bugs.chromium.org/p/chromium/issues/detail?id=426798
        self:close()
        return
    elseif flag == WS_OP_PING then
        -- 返回pong时，如果对方ping时发了body，一定要原封不动返回
        local mask = self.ws_key and CLT_MASK or SRV_MASK
        return network_mgr:send_ctrl_packet(
            self.conn_id, WS_OP_PONG | mask, body)
    elseif flag == WS_OP_PONG then
        -- TODO: 这里更新心跳
        return
    end

    assert(false, "unknow websocket ctrl flag " .. flag)
end

return WsConn
