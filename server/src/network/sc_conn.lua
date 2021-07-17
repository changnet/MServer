local network_mgr = network_mgr
local Conn = require "network.conn"

-- 与客户端的连接
local ScConn = oo.class(..., Conn)

ScConn.default_param = {
    iot = network_mgr.IOT_NONE, -- io类型
    cdt = network_mgr.CDT_PROTOBUF, -- 编码类型
    pkt = network_mgr.PT_STREAM, -- 打包类型
    action = 1, -- over_action，1 表示缓冲区溢出后断开
    chunk_size = 8192, -- 单个缓冲区大小
    send_chunk_max = 128, -- 发送缓冲区数量
    recv_chunk_max = 8 -- 接收缓冲区数
}

function ScConn:__init(conn_id)
    self.conn_id = conn_id
end

-- 发送数据包
function ScConn:send_pkt(cmd, pkt, errno)
    -- 使用tcp二进制流
    return network_mgr:send_clt_packet(self.conn_id, cmd.i, errno or 0, pkt)
    -- 使用websocket
    -- return network_mgr:send_clt_packet(
    --     self.conn_id,cmd,errno or 0,WS_OP_BINARY | WS_FINAL_FRAME,pkt )
end

-- 认证成功
function ScConn:authorized()
    self.auth = true
end

-- 将该链接绑定一个角色
function ScConn:bind_role(pid)
    self.pid = pid
    network_mgr:set_conn_owner(self.conn_id, self.pid or 0)
end

-- 监听客户端连接
function ScConn:listen(ip, port)
    self.conn_id = network_mgr:listen(ip, port, network_mgr.CT_SCCN)

    self:set_conn(self.conn_id, self)
end

-- 连接断开
function ScConn:conn_del()
    -- 这个会自动解除
    -- self:set_conn( self.conn_id,nil )
    network_mgr:unset_conn_owner(self.conn_id, self.pid or 0)

    return g_clt_mgr:clt_conn_del(self.conn_id)
end

-- 消息回调
function ScConn:command_new(cmd, ...)
    return Cmd.dispatch_clt(self, cmd, ...)
end

-- 主动关闭连接
function ScConn:close()
    self:set_conn(self.conn_id, nil)
    network_mgr:unset_conn_owner(self.conn_id, self.pid or 0)

    return network_mgr:close(self.conn_id)
end

-- 接受新客户端连接
function ScConn:conn_accept(new_conn_id)
    local new_conn = ScConn(new_conn_id)

    new_conn:set_conn_param()
    g_clt_mgr:clt_conn_accept(new_conn_id, new_conn)

    return new_conn
end

return ScConn
