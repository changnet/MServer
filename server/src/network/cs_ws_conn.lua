local network_mgr = network_mgr
local CsConn = require "network.cs_conn"
local WsConn = require "network.ws_conn"

-- 客户端与服务器的连接， 采用websocket打包，用于机器人和单元测试
local CsWsConn = oo.class(..., CsConn, WsConn)

CsWsConn.default_param = {
    listen_type = network_mgr.CT_SCCN, -- 监听的连接类型
    connect_type = network_mgr.CT_CSCN, -- 连接类型
    iot = network_mgr.IOT_NONE, -- io类型
    cdt = network_mgr.CDT_PROTOBUF, -- 编码类型
    pkt = network_mgr.PT_WSSTREAM, -- 打包类型
    action = 1, -- over_action，1 表示缓冲区溢出后断开
    chunk_size = 8192, -- 单个缓冲区大小
    send_chunk_max = 128, -- 发送缓冲区数量
    recv_chunk_max = 8 -- 接收缓冲区数
}

local MASK = WsConn.WS_FINAL_FRAME | WsConn.WS_HAS_MASK | WsConn.WS_OP_BINARY

function CsWsConn:__init(conn_id)
    self.conn_id = conn_id
end

-- 发送数据包
function CsWsConn:send_pkt(cmd, pkt)
    return network_mgr:send_srv_packet(self.conn_id, cmd.i, MASK, pkt)
end

-- 主动关闭连接
function CsWsConn:close()
    self:ws_close()

    -- 把WS_OP_CLOSE这个包发出去
    CsConn.close(self, true)
end

return CsWsConn
