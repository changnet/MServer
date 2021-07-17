local util = require "util"
local Conn = require "network.conn"
local HttpConn = require "http.http_conn"

-- websocket公共层
local WsConn = oo.class(..., Conn)

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

-- 导出一些常量
WSConn = {
    WS_OP_CONTINUE = WS_OP_CONTINUE,
    WS_OP_TEXT = WS_OP_TEXT,
    WS_OP_BINARY = WS_OP_BINARY,
    WS_OP_CLOSE = WS_OP_CLOSE,
    WS_OP_PING = WS_OP_PING,
    WS_OP_PONG = WS_OP_PONG,
    WS_FINAL_FRAME = WS_FINAL_FRAME,
    WS_HAS_MASK = WS_HAS_MASK,
}

-- 处理websocket握手
function WsConn:handshake_new(sec_websocket_key, sec_websocket_accept)
    -- 服务器收到客户端的握手请求
    if not sec_websocket_key then
        self.close()
        PRINTF("clt handshake no sec_websocket_key")
        return
    end

    local sha1 = util.sha1_raw(sec_websocket_key, ws_magic)
    local base64 = util.base64(sha1)

    PRINTF("clt handshake %d", self.conn_id)
    return network_mgr:send_raw_packet(self.conn_id,
                                       string.format(handshake_srv, base64))
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

-- 发送原生数据包
-- @param body 要发送的文字
function WsConn:send_pkt(body)
    return network_mgr:send_clt_packet(
        self.conn_id, WS_OP_TEXT | WS_FINAL_FRAME, body)
end

-- 发送控制包
-- @param mask 控制包掩码，按位表示，如 WS_OP_PONG | WS_FINAL_FRAME
-- @param body 控制包的数据，可以为nil
function WsConn:send_ctrl(mask, body)
    network_mgr:send_ctrl_packet(self.conn_id, WS_OP_PING, body)
end

function WsConn:ws_close()
    self.op_close = true
    self:send_ctrl(WS_OP_CLOSE)
end

-- 处理控制包
function WsConn:ctrl_new(flag, body)
    -- 控制帧只在前4位，先去掉WS_HAS_MASK
    flag = flag & 0x0F
    if flag == WS_OP_CLOSE then
        if self.op_close then return end

        network_mgr:send_ctrl_packet(self.conn_id,
                                            WS_OP_CLOSE | WS_FINAL_FRAME)
        -- RFC6455 5.5.1
        -- If an endpoint receives a Close frame and did not previously send a
        -- Close frame, the endpoint MUST send a Close frame in response
        -- The server MUST close the underlying TCP connection immediately;
        -- 在chrome中，调用websocket的close函数后，如果服务器不主动关闭，可能会几分钟
        -- 后才会关闭连接
        -- https://bugs.chromium.org/p/chromium/issues/detail?id=426798
        self:close()
        self:conn_del() -- 这个属于对方关闭连接，需要触发关闭事件
        return
    elseif flag == WS_OP_PING then
        -- 返回pong时，如果对方ping时发了body，一定要原封不动返回
        return network_mgr:send_ctrl_packet(self.conn_id,
                                            WS_OP_PONG | WS_FINAL_FRAME, body)
    elseif flag == WS_OP_PONG then
        -- TODO: 这里更新心跳
        return
    end

    assert(false, "unknow websocket ctrl flag")
end

return WsConn
