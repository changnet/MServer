local network_mgr = network_mgr
local Conn = require "network.conn"

-- 客户端与服务器的连接，用于机器人和单元测试
local CsConn = oo.class(..., Conn)

CsConn.default_param = {
    iot = network_mgr.IOT_NONE, -- io类型
    cdt = network_mgr.CDT_PROTOBUF, -- 编码类型
    pkt = network_mgr.PT_STREAM, -- 打包类型
    action = 1, -- over_action，1 表示缓冲区溢出后断开
    chunk_size = 8192, -- 单个缓冲区大小
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

-- 客户端连接到服务器
function CsConn:connect(ip, port)
    self.ip = ip
    self.port = port

    self.ok = false
    self.conn_id = network_mgr:connect(self.ip, self.port, network_mgr.CT_CSCN)

    self:set_conn(self.conn_id, self)

    return self.conn_id
end

function CsConn:conn_new(ecode)
    if 0 == ecode then
        self:set_conn_param()
    end
end

-- 消息回调
function CsConn:command_new(cmd, ...)
    -- 这个需要重载，这里没法定义逻辑
    assert(false)
end

-- 主动关闭连接
function CsConn:close()
    self:set_conn(self.conn_id, nil)

    return network_mgr:close(self.conn_id)
end

return CsConn
