local network_mgr = network_mgr
local CsConn = require "network.cs_conn"
local WsConn = require "network.ws_conn"

-- 客户端与服务器的连接， 采用websocket打包，用于机器人和单元测试
local CsWsConn = oo.class(..., CsConn, WsConn)

CsWsConn.default_param = {
    iot = network_mgr.IOT_NONE, -- io类型
    cdt = network_mgr.CDT_PROTOBUF, -- 编码类型
    pkt = network_mgr.PT_WSSTREAM, -- 打包类型
    action = 1, -- over_action，1 表示缓冲区溢出后断开
    chunk_size = 8192, -- 单个缓冲区大小
    send_chunk_max = 128, -- 发送缓冲区数量
    recv_chunk_max = 8 -- 接收缓冲区数
}

function CsWsConn:__init(conn_id)
    self.conn_id = conn_id
end

-- 发送数据包
function CsWsConn:send_pkt(cmd, pkt)
    return network_mgr:send_srv_packet(self.conn_id, cmd.i, pkt)
end

-- 连接到其他服务器
-- @param host 目标服务器地址(支持域名)
-- @param port 目标服务器端口
function CsWsConn:connect(host, port)
    local ip = util.get_addr_info(host)
    -- 这个host需要注意，实测对www.example.com请求时，如果host为一个ip，是会返回404的
    self.host = host

    CsConn.connect(self, ip, port)
end

-- 以https试连接到其他服务器
-- @param host 目标服务器地址
-- @param port 目标服务器端口
-- @param ssl 用new_ssl_ctx创建的ssl_ctx
function CsWsConn:connect_s(host, port, ssl)
    self.ssl = assert(ssl)

    return self:connect(host, port)
end

-- 主动关闭连接
function CsWsConn:close()
    self:ws_close()
    self:set_conn(self.conn_id, nil)

    return network_mgr:close(self.conn_id, true) -- 把WS_OP_CLOSE这个包发出去
end

return CsWsConn
