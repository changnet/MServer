local network_mgr = network_mgr
local Conn = require "network.conn"

-- 与客户端的连接
local ScConn = oo.class(..., Conn)

ScConn.default_param = {
    iot = network_mgr.IOT_NONE, -- io类型
    pkt = network_mgr.PT_STREAM, -- 打包类型
    action = 1, -- over_action，1 表示缓冲区溢出后断开
    send_chunk_max = 128, -- 发送缓冲区数量
    recv_chunk_max = 8 -- 接收缓冲区数
}

function ScConn:__init(socket_id)
    self.socket_id = socket_id
end

-- 发送数据包
function ScConn:send_pkt(cmd, pkt, errno)
    return network_mgr:send_clt_packet(self.socket_id, cmd.i, errno or 0, pkt)
end

-- 认证成功
function ScConn:authorized()
    self.auth = true
end

-- 将该链接绑定一个角色
function ScConn:bind_role(pid)
    self.pid = pid
    network_mgr:set_conn_owner(self.socket_id, self.pid or 0)
end

-- 连接断开
function ScConn:on_disconnected()
    network_mgr:unset_conn_owner(self.socket_id, self.pid or 0)

    return CltMgr.clt_conn_del(self.socket_id)
end

-- 消息回调
function ScConn:on_message(cmd, ...)
    return Cmd.dispatch_clt(self, cmd, ...)
end

-- 主动关闭连接
function ScConn:close(flush)
    network_mgr:unset_conn_owner(self.socket_id, self.pid or 0)

    return Conn.close(self, flush)
end

-- 接受新客户端连接
function ScConn:on_accepted()
    CltMgr.clt_accept(self)
end

return ScConn
