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

function ScSocket:__init(socket_id)
    WebSocket.__init(self)
end

-- 发送数据包
function ScSocket:send_pkt(cmd, pkt, errno)
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
    return Cmd.dispatch_clt(self, cmd, ...)
end

-- 接受新客户端连接
function ScSocket:on_accepted()
    CltMgr.add(self)
end

return ScSocket
