local network_mgr = network_mgr
local CsConn = require "network.cs_conn"
local WsConn = require "network.ws_conn"

-- 客户端与服务器的连接， 采用websocket打包，用于机器人和单元测试
local CsWsConn = oo.class(..., CsConn, WsConn)

function CsWsConn:__init(conn_id)
    self.conn_id = conn_id
end

function CsWsConn:set_conn_param(conn_id)
    network_mgr:set_conn_io(conn_id, network_mgr.IOT_NONE)
    network_mgr:set_conn_codec(conn_id, network_mgr.CDT_PROTOBUF)

    -- 使用tcp二进制流
    network_mgr:set_conn_packet(conn_id, network_mgr.PT_STREAM)
    -- 使用websocket二进制流
    -- network_mgr:set_conn_packet( conn_id,network_mgr.PT_WSSTREAM )

    -- set_send_buffer_size最后一个参数表示over_action，1 = 溢出后断开
    network_mgr:set_send_buffer_size(conn_id, 128, 8192, 1) -- 8k*128 = 1024k
    network_mgr:set_recv_buffer_size(conn_id, 8, 8192) -- 8k*8 = 64k
end

-- 发送数据包
function CsWsConn:send_pkt(cmd, pkt)
    return network_mgr:send_srv_packet(self.conn_id, cmd.i, pkt)
end

-- 连接到其他服务器
-- @param host 目标服务器地址(支持域名)
-- @param port 目标服务器端口
function CsWsConn:connect(host, port, on_connect, on_command)
    local ip = util.get_addr_info(host)
    -- 这个host需要注意，实测对www.example.com请求时，如果host为一个ip，是会返回404的
    self.host = host
    self.port = port
    CsConn.connect(self, ip, port)
end

-- 主动关闭连接
function CsWsConn:close()
    self:ws_close()

    self:set_conn(self.conn_id, nil)

    return network_mgr:close(self.conn_id, true) -- 把WS_OP_CLOSE这个包发出去
end

return CsWsConn
