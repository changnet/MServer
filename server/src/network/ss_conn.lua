-- srv_conn server connection
local network_mgr = network_mgr

local Conn = require "network.conn"

-- 服务器之间的链接
local SsConn = oo.class(..., Conn)

SsConn.default_param = {
    listen_type = network_mgr.CT_SSCN, -- 监听的连接类型
    connect_type = network_mgr.CT_SSCN, -- 连接类型
    iot = network_mgr.IOT_NONE, -- io类型
    cdt = network_mgr.CDT_PROTOBUF, -- 编码类型
    pkt = network_mgr.PT_STREAM, -- 打包类型
    action = 2, -- over_action，2 表示缓冲区满后进入自旋来发送数据
    chunk_size = 8 * 1024 * 1024, -- 单个缓冲区大小
    send_chunk_max = 64, -- 发送缓冲区数量
    recv_chunk_max = 1 -- 接收缓冲区数
}

function SsConn:__init(conn_id)
    self.auth = false
    self.beat = 0
    self.fchk = 0 -- fail check
    self.session = 0
    self.conn_id = conn_id -- accept才需要传该conn_id，listen、connect会自动产生一个新的
    self.auto_conn = false -- 是否自动重连
end

-- 发送数据包
function SsConn:send_pkt(cmd, pkt, ecode)
    return network_mgr:send_s2s_packet(self.conn_id, cmd.i, ecode or 0, pkt)
end

-- 给客户端发送数据包 !!!当前连接必须是网关链接!!!
function SsConn:send_clt_pkt(pid, cmd, pkt, ecode)
    return network_mgr:send_ssc_packet(self.conn_id, pid,
                                       network_mgr.CDT_PROTOBUF, cmd.i,
                                       ecode or 0, pkt)
end

-- 发送数据包
function SsConn:send_rpc_pkt(unique_id, method_name, ...)
    return
        network_mgr:send_rpc_packet(self.conn_id, unique_id, method_name, ...)
end

-- timeout check
function SsConn:check(check_time)
    if self.beat < check_time then
        self.fchk = self.fchk + 1

        return self.fchk
    end

    self.fchk = 0
    return 0
end

-- 认证成功
function SsConn:authorized(pkt)
    self.auth = true
    self.session = pkt.session
end

-- 获取该连接的app类型
function SsConn:session_info()
    return g_app:decode_session(self.session)
end

-- 获取基本类型名字(gateway、world)，见APP服务器类型定义，通常用于打印日志
function SsConn:base_name(session_type)
    if not session_type then
        session_type = g_app:decode_session(self.session)
    end
    for name, ty in pairs(APP) do
        if ty == session_type then return name end
    end
    return nil
end

-- 获取该连接名称，包括基础名字，索引，服务器id，通常用于打印日志
function SsConn:conn_name(session)
    -- 该服务器连接未经过认证
    if 0 == session then return "unauthorized" end

    local ty, index, id = g_app:decode_session(session or self.session)

    return string.format("%s(I%d.S%d)", self:base_name(ty), index, id)
end

-- 重新连接
function SsConn:reconnect()
    self.auth = false
    self.session = 0

    return self:connect(self.ip, self.port)
end

-- 接受新的连接
function SsConn:on_accepted()
    g_srv_mgr:srv_conn_accept(self)
end

-- 连接成功
function SsConn:on_connected()
    return g_srv_mgr:on_conn_ok(self.conn_id)
end

-- 连接断开
function SsConn:on_disconnected()
    return g_srv_mgr:srv_conn_del(self.conn_id)
end

-- 服务器之间消息回调
function SsConn:on_cmd(session, cmd, errno, pkt)
    self.beat = ev:time()
    return Cmd.dispatch_srv(self, cmd, pkt)
end

-- 转发的客户端消息
function SsConn:css_command_new(pid, cmd, ...)
    return Cmd:dispatch_css(self, pid, cmd, ...)
end

return SsConn
