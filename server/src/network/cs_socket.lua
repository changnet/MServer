local WebSocket = require "network.websocket"
local EngineSocket = require "engine.Socket"
local NetHeader = require "message.net_header"

-- 客户端与服务器的连接，用于机器人和单元测试
local CsSocket = oo.class("CsSocket", WebSocket)

CsSocket.default_param = {
    pkt = EngineSocket.PT_WEBSOCKET, -- 打包类型
    action = 1, -- over_action，1 表示缓冲区溢出后断开
    send_byte_max = 1024 * 1024, -- 发送缓冲区数量
    recv_byte_max = 1024 * 1024 -- 接收缓冲区数
}

local MASK = CsSocket.WS_OP_BINARY | CsSocket.WS_FINAL_FRAME |
                 CsSocket.WS_HAS_MASK

function CsSocket:__init(socket_id)
    WebSocket.__init(self)
end

function CsSocket:set_option()
    self.s:set_nodelay(1)
end

-- 发送数据包
-- @param msg_id 协议号（业务头）
-- @param buffer string或者lightuserdata
-- @param size buffer的长度，buffer为lightuserdata时必填
function CsSocket:send_pkt(msg_id, buffer, size)
    local hp, hl = NetHeader.encode("ws", msg_id)
    return self.s:send_pkt(MASK, buffer, size, hp, hl)
end

return CsSocket
