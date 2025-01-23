local WebSocket = require "network.websocket"
local EngineSocket = require "engine.Socket"

-- 客户端与服务器的连接，用于机器人和单元测试
local CsSocket = oo.class(WebSocket)

CsSocket.default_param = {
    pkt = EngineSocket.PT_WSSTREAM, -- 打包类型
    action = 1, -- over_action，1 表示缓冲区溢出后断开
    send_chunk_max = 128, -- 发送缓冲区数量
    recv_chunk_max = 8 -- 接收缓冲区数
}

local MASK = CsSocket.WS_OP_BINARY | CsSocket.WS_FINAL_FRAME | CsSocket.WS_HAS_MASK

function CsSocket:__init(socket_id)
    WebSocket.__init(self)
end

-- 发送数据包
function CsSocket:send_pkt(msg_id, buffer, size)
    return self.s:send_clt(MASK, msg_id, buffer, size)
end

return CsSocket
