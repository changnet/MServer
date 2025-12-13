local WebSocket = require "network.websocket"
local EngineSocket = require "engine.Socket"

-- 服务器与客户端的连接
local ScSocket = oo.class("ScSocket", WebSocket)

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

function ScSocket:set_option()
    self.s:set_nodelay(1)

    -- 保活和超时现在一般通过心跳包实现，不用tcp的机制
    -- self.s:set_user_timeout(60000)
    -- self.s:set_keep_alive(60, 10, 5)
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

-- 连接断开
function ScSocket:on_disconnected()
    return CltMgr.del(self)
end

-- 消息回调
function ScSocket:on_message(cmd, buffer, size)
    return NetMsg.dispatch(self, cmd, buffer, size)
end

-- 接受新客户端连接
function ScSocket:on_accepted()
    CltMgr.add(self)
end

return ScSocket
