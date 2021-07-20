local network_mgr = network_mgr

local ScConn = require "network.sc_conn"
local WsConn = require "network.ws_conn"

-- 与客户端的连接，采用 websocket打包
local ScWsConn = oo.class(..., ScConn, WsConn)

ScWsConn.default_param = {
    iot = network_mgr.IOT_NONE, -- io类型
    cdt = network_mgr.CDT_PROTOBUF, -- 编码类型
    pkt = network_mgr.PT_WSSTREAM, -- 打包类型
    action = 1, -- over_action，1 表示缓冲区溢出后断开
    chunk_size = 8192, -- 单个缓冲区大小
    send_chunk_max = 128, -- 发送缓冲区数量
    recv_chunk_max = 8 -- 接收缓冲区数
}

local MASK = WsConn.WS_FINAL_FRAME | WsConn.WS_OP_BINARY

function ScWsConn:__init(conn_id)
    self.conn_id = conn_id
end

-- 发送数据包
function ScWsConn:send_pkt(cmd, pkt, errno)
    return network_mgr:send_clt_packet(
        self.conn_id, cmd.i, errno or 0, MASK, pkt)
end

-- 主动关闭连接
function ScWsConn:close()
    self:ws_close()
    self:set_conn(self.conn_id, nil)
    network_mgr:unset_conn_owner(self.conn_id, self.pid or 0)

    return network_mgr:close(self.conn_id, true) -- -- 把WS_OP_CLOSE这个包发出去
end

return ScWsConn
