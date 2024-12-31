local network_mgr = network_mgr
local Conn = require "network.conn"

-- 客户端与服务器的连接，用于机器人和单元测试
local CsConn = oo.class(..., Conn)

CsConn.default_param = {
    iot = network_mgr.IOT_NONE, -- io类型
    pkt = network_mgr.PT_STREAM, -- 打包类型
    action = 1, -- over_action，1 表示缓冲区溢出后断开
    send_chunk_max = 128, -- 发送缓冲区数量
    recv_chunk_max = 8 -- 接收缓冲区数
}

function CsConn:__init(conn_id)
    self.conn_id = conn_id
end

-- 发送数据包
function CsConn:send_pkt(cmd, pkt)
    return network_mgr:send_srv_packet(self.conn_id, cmd.i, pkt)
end

return CsConn
