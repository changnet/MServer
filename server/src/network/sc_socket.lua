local WebSocket = require "network.websocket"
local EngineSocket = require "engine.Socket"
local NetHeader = require "message.net_header"

-- 服务器与客户端的连接
local ScSocket = oo.class("ScSocket", WebSocket)

ScSocket.default_param = {
    pkt = EngineSocket.PT_WEBSOCKET, -- 打包类型
    action = 1, -- over_action，1 表示缓冲区溢出后断开
    send_byte_max = 1024 * 1024, -- 发送缓冲区数量
    recv_byte_max = 1024 * 1024 -- 接收缓冲区数
}

local MASK = ScSocket.WS_OP_BINARY | ScSocket.WS_FINAL_FRAME

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
-- @param msg_id 协议号（业务头）
-- @param buffer string或者lightuserdata
-- @param size buffer的长度，buffer为lightuserdata时必填
function ScSocket:send_pkt(msg_id, buffer, size)
    local hp, hl = NetHeader.encode("ws", msg_id)
    return self.s:send_pkt(MASK, buffer, size, hp, hl)
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
-- @param ud 载荷指针，只在本次回调内有效
-- @param size 载荷长度
function ScSocket:on_message(ud, size)
    local cmd, buffer, bsize = NetHeader.decode("ws", ud, size)
    if not cmd then return self:close() end

    return NetMsg.dispatch(self, cmd, buffer, bsize)
end

-- 接受新客户端连接
function ScSocket:on_accepted()
    CltMgr.add(self)
end

return ScSocket
