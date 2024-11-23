-- srv_conn server connection
local network_mgr = network_mgr

local Conn = require "network.conn"

-- 服务器之间的链接
local SsConn = oo.class(..., Conn)

SsConn.default_param = {
    iot = network_mgr.IOT_NONE, -- io类型
    pkt = network_mgr.PT_STREAM, -- 打包类型
    action = 2, -- over_action，2 表示缓冲区满后进入自旋来发送数据
    send_chunk_max = 1024, -- 发送缓冲区chunk数量，单个chunk8k，见C++ buffer.h定义
    recv_chunk_max = 1024 -- 接收缓冲区chunk数量
}

function SsConn:__init(conn_id)
    self.auth = false
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

-- 认证成功
function SsConn:authorized(pkt)
    self.auth = true
    self.session = pkt.session

    self.app_type, self.app_index, self.app_id
        = App.decode_session(self.session)

    for name, ty in pairs(APP) do
        if ty == self.app_type then
            self.name = name
            break
        end
    end
end

-- 获取基本类型名字(gateway、world)，见APP服务器类型定义，通常用于打印日志
function SsConn:base_name()
    -- 如果已经注册成功，则使用已注册的名字
    if self.name then return self.name end

    -- 连接过来的socket，会执行名字注册，但如果报错可能注册不成功
    if not self.host then return "unknow" end

    -- 没有注册的情况下，使用地址作为标识名，方便查问题
    return string.format("%s:%d", self.host, self.port)
end

-- 获取该连接名称，包括基础名字，索引，服务器id，通常用于打印日志
function SsConn:conn_name()
    return string.format("%s(I%d.S%d)",
        self:base_name(), self.app_index or 0, self.app_id or 0)
end

-- 接受新的连接
function SsConn:on_accepted()
    SrvMgr.srv_conn_accept(self)
end

-- 连接成功
function SsConn:on_connected()
    return SrvMgr.on_conn_ok(self.conn_id)
end

-- 连接断开
function SsConn:on_disconnected(e, is_conn)
    self.auth = false
    return SrvMgr.srv_conn_del(self.conn_id, e, is_conn)
end

-- 服务器之间消息回调
function SsConn:on_cmd(session, cmd, errno, pkt)
    return Cmd.dispatch_srv(self, cmd, pkt)
end

-- 转发的客户端消息
function SsConn:css_command_new(pid, cmd, ...)
    return Cmd.dispatch_css(self, pid, cmd, ...)
end

return SsConn
