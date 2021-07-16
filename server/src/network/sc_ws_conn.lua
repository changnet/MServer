local network_mgr = network_mgr

local Conn = require "network.conn"
local WsConn = require "network.ws_conn"

-- 与客户端的连接，采用 websocket打包
local ScConn = oo.class(..., Conn, WsConn)

local WS_OP_BINARY = WsConn.WS_OP_BINARY
local WS_FINAL_FRAME = WsConn.WS_FINAL_FRAME

function ScConn:__init(conn_id)
    self.conn_id = conn_id
end

function ScConn:set_conn_param(conn_id)
    network_mgr:set_conn_io(conn_id, network_mgr.IOT_NONE)
    network_mgr:set_conn_codec(conn_id, network_mgr.CT_PROTOBUF)

    -- 使用tcp二进制流
    network_mgr:set_conn_packet(conn_id, network_mgr.PT_WSSTREAM)

    -- set_send_buffer_size最后一个参数表示over_action，1 = 溢出后断开
    network_mgr:set_send_buffer_size(conn_id, 128, 8192, 1) -- 8k*128 = 1024k
    network_mgr:set_recv_buffer_size(conn_id, 8, 8192) -- 8k*8 = 64k
end

-- 发送数据包
function ScConn:send_pkt(cmd, pkt, errno)
    -- 使用websocket
    return network_mgr:send_clt_packet(
        self.conn_id,cmd,errno or 0, WS_OP_BINARY | WS_FINAL_FRAME, pkt)
end

-- 主动关闭连接
function ScConn:close()
    self:ws_close()
    self:set_conn(self.conn_id, nil)
    network_mgr:unset_conn_owner(self.conn_id, self.pid or 0)

    return network_mgr:close(self.conn_id, true) -- -- 把WS_OP_CLOSE这个包发出去
end

return ScConn
