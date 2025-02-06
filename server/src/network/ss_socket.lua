
local Socket = require "network.socket"
local EngineSocket = require "engine.Socket"

-- 服务器之间的链接
local SsSocket = oo.class(Socket)

SsSocket.default_param = {
    pkt = EngineSocket.PT_SSSTREAM, -- 打包类型
    action = 2, -- over_action，2 表示缓冲区满后进入自旋来发送数据
    send_chunk_max = 1024, -- 发送缓冲区chunk数量，单个chunk8k，见C++ buffer.h定义
    recv_chunk_max = 1024 -- 接收缓冲区chunk数量
}

local send_srv = EngineSocket.send_srv

function SsSocket:__init()
    Socket.__init(self)

    self.auth = false
    self.session = 0
    self.auto_conn = false -- 是否自动重连
end

-- 发送数据包
function SsSocket:send_pkt(src, dst, mtype, usize, udata)
    return send_srv(self.s, src, dst, mtype, usize, udata)
end

-- 认证成功
function SsSocket:authorized(pkt)

end

-- 获取基本类型名字(gateway、world)，见APP服务器类型定义，通常用于打印日志
function SsSocket:base_name()
    -- 如果已经注册成功，则使用已注册的名字
    if self.name then return self.name end

    -- 连接过来的socket，会执行名字注册，但如果报错可能注册不成功
    if not self.host then return "unknow" end

    -- 没有注册的情况下，使用地址作为标识名，方便查问题
    return string.format("%s:%d", self.host, self.port)
end

-- 获取该连接名称，包括基础名字，索引，服务器id，通常用于打印日志
function SsSocket:conn_name()
    return string.format("%s(I%d.S%d)",
        self:base_name(), self.app_index or 0, self.app_id or 0)
end

-- 接受新的连接
function SsSocket:on_accepted()
    SrvMgr.srv_accept(self)
end

-- 连接成功
function SsSocket:on_connected()
    return SrvMgr.on_conn_ok(self.socket_id)
end

-- 连接断开
function SsSocket:on_disconnected()
    self.auth = false
    -- return SrvMgr.srv_conn_del(self.socket_id, e, is_conn)
end

-- 服务器之间消息回调
function SsSocket:on_message(src, dst, mtype, usize, udata)
    -- return Cmd.dispatch_srv(self, cmd, pkt)
end

return SsSocket
