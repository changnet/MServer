-- srv_conn server connection
local network_mgr = network_mgr

local Conn = require "network.conn"

-- 服务器之间的链接
local SrvConn = oo.class(..., Conn)

function SrvConn:__init(conn_id)
    self.auth = false
    self.beat = 0
    self.fchk = 0 -- fail check
    self.session = 0
    self.conn_id = conn_id -- accept才需要传该conn_id，listen、connect会自动产生一个新的
    self.auto_conn = false -- 是否自动重连
end

-- 发送数据包
function SrvConn:send_pkt(cmd, pkt, ecode)
    return network_mgr:send_s2s_packet(self.conn_id, cmd.i, ecode or 0, pkt)
end

-- 给客户端发送数据包 !!!当前连接必须是网关链接!!!
function SrvConn:send_clt_pkt(pid, cmd, pkt, ecode)
    return network_mgr:send_ssc_packet(self.conn_id, pid,
                                       network_mgr.CDC_PROTOBUF, cmd.i,
                                       ecode or 0, pkt)
end

-- 发送数据包
function SrvConn:send_rpc_pkt(unique_id, method_name, ...)
    return
        network_mgr:send_rpc_packet(self.conn_id, unique_id, method_name, ...)
end

-- timeout check
function SrvConn:check(check_time)
    if self.beat < check_time then
        self.fchk = self.fchk + 1

        return self.fchk
    end

    self.fchk = 0
    return 0
end

-- 认证成功
function SrvConn:authorized(pkt)
    self.auth = true
    self.session = pkt.session
end

-- 获取基本类型名字(gateway、world)，见APP服务器类型定义，通常用于打印日志
function SrvConn:base_name(session_type)
    if not session_type then
        session_type = g_app:srv_session_parse(self.session)
    end
    for name, ty in pairs(APP) do
        if ty == session_type then return name end
    end
    return nil
end

-- 获取该连接名称，包括基础名字，索引，服务器id，通常用于打印日志
function SrvConn:conn_name(session)
    -- 该服务器连接未经过认证
    if 0 == session then return "unauthorized" end

    local ty, index, id = g_app:srv_session_parse(session or self.session)

    return string.format("%s(I%d.S%d)", self:base_name(ty), index, id)
end

-- 监听服务器连接
function SrvConn:listen(ip, port)
    self.conn_id = network_mgr:listen(ip, port, network_mgr.CNT_SSCN)

    self:set_conn(self.conn_id, self)
end

function SrvConn:raw_connect()
    self.ok = false
    self.conn_id = network_mgr:connect(self.ip, self.port, network_mgr.CNT_SSCN)

    self:set_conn(self.conn_id, self)

    return self.conn_id
end

-- 连接到其他服务器
function SrvConn:connect(ip, port)
    self.ip = ip
    self.port = port

    return self:raw_connect()
end

-- 重新连接
function SrvConn:reconnect()
    self.auth = false
    self.session = 0

    return self:raw_connect()
end

-- 重新连接
function SrvConn:set_conn_param(conn_id)
    network_mgr:set_conn_io(conn_id, network_mgr.IOT_NONE)
    network_mgr:set_conn_codec(conn_id, network_mgr.CDC_PROTOBUF)
    network_mgr:set_conn_packet(conn_id, network_mgr.PKT_STREAM)

    -- 设置服务器之间链接缓冲区大小：
    -- 发送的话可能会累加，要设置大些.16777216 = 16MB，最大累加16*64 = 1024M
    -- 接收的话现在是收到数据立马解析完，不需要很大
    -- set_send_buffer_size最后一个参数表示over_action，2 = 溢出后阻塞
    network_mgr:set_send_buffer_size(conn_id, 64, 16777216, 2)
    network_mgr:set_recv_buffer_size(conn_id, 8, 8388608) -- 8M
end

-- 接受新的连接
function SrvConn:conn_accept(new_conn_id)
    self:set_conn_param(new_conn_id)

    local new_conn = SrvConn(new_conn_id)
    g_srv_mgr:srv_conn_accept(new_conn_id, new_conn)

    return new_conn
end

-- 连接进行初始化
function SrvConn:conn_new(ecode)
    if 0 == ecode then
        self.ok = true
        self:set_conn_param(self.conn_id)
    else
        return g_srv_mgr:srv_conn_new(self.conn_id, ecode)
    end
end

-- 连接成功
function SrvConn:conn_ok()
    return g_srv_mgr:srv_conn_new(self.conn_id, 0)
end

-- 连接断开
function SrvConn:conn_del()
    self.ok = false
    return g_srv_mgr:srv_conn_del(self.conn_id)
end

-- 服务器之间消息回调
function SrvConn:command_new(session, cmd, errno, pkt)
    self.beat = ev:time()
    return Cmd:dispatch_srv(self, cmd, pkt)
end

-- 转发的客户端消息
function SrvConn:css_command_new(pid, cmd, ...)
    return Cmd:dispatch_css(self, pid, cmd, ...)
end

-- 主动关闭连接
function SrvConn:close()
    self.ok = false
    return network_mgr:close(self.conn_id)
end

-- 发送认证数据
function SrvConn:send_register()
    local pkt = {
        session = g_app.session,
        timestamp = ev:time()
    }
    pkt.auth = util.md5(SRV_KEY, pkt.timestamp, pkt.session)
    self:send_pkt(SYS.REG, pkt)
end

return SrvConn
