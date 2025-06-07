local WebSocket = require "network.websocket"
local EngineSocket = require "engine.Socket"

-- 服务器与客户端的连接
local ScSocket = oo.class(WebSocket)

ScSocket.default_param = {
    pkt = EngineSocket.PT_WSSTREAM, -- 打包类型
    action = 1, -- over_action，1 表示缓冲区溢出后断开
    send_chunk_max = 128, -- 发送缓冲区数量
    recv_chunk_max = 8 -- 接收缓冲区数
}

local MASK = ScSocket.WS_OP_BINARY | ScSocket.WS_FINAL_FRAME
local send_srv = EngineSocket.send_srv

function ScSocket:__init(socket_id)
    WebSocket.__init(self)
end

-- 发送数据包
function ScSocket:send_pkt(msg_id, buffer, size)
    -- self.s:send_srv(MASK, msg_id, buffer, size) -- 这个要访问元表慢一些
    return send_srv(self.s, MASK, msg_id, buffer, size)
end

-- 认证成功
function ScSocket:authorized()
    self.auth = true
end

-- 将该链接绑定一个角色
function ScSocket:bind_role(pid)
    self.pid = pid
end

-- 连接断开
function ScSocket:on_disconnected()
    return CltMgr.del(self.socket_id)
end

-- 消息回调
function ScSocket:on_message(cmd, ...)
    -- 这里回调的参数，取决于socket的类型的unpack函数
    -- websocket stream，则是cmd, buffer, size
    return Cmd.dispatch_clt(self, cmd, ...)
end

-- 接受新客户端连接
function ScSocket:on_accepted()
    CltMgr.add(self)
end

return ScSocket
