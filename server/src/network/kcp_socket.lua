local EngineSocket = require "engine.Socket"
local Socket = require "network.socket"

local KcpSocket = oo.class("KcpSocket", Socket)

KcpSocket.MAX_MSG = 64 * 1024

KcpSocket.default_param = {
    pkt = EngineSocket.PT_KCPSTREAM, -- 打包类型
    io_type = EngineSocket.IOT_KCP, -- 读写方式
    -- over_action，缓冲区溢出后的处理方式
    -- kcp下每个对端是一条独立连接，缓冲区溢出只影响它自己，可以断开。
    -- 溢出判定读的是这条连接自己的 EVIO 的 mask_（服务端对端是那个虚拟EVIO），
    -- 而不是监听主EVIO的，所以一个暴力对端不会影响其他对端
    action = 1,
    send_byte_max = 1024 * 1024, -- 发送缓冲区数量
    recv_byte_max = 1024 * 1024, -- 接收缓冲区数量
}

--[[
kcp没有自己的fd，三种形态都走 Socket 的公共流程，不额外开接口：
    客户端          KcpSocket()                     自己建fd，EV_CONNECT
    监听            KcpSocket():listen(ip, port)     真实fd，EV_ACCEPT
    服务端对端      accept返回vfd → on_accepting(vfd) → start_event(vfd, EV_READ)

vfd 编码了所属监听socket（高位是socket_id，低位是序号）。后端收到它就能
把这条对端挂到那个监听的accept表上，所以lua不需要知道对端地址是什么。
]]

-- 关闭链接
-- @param flush 关闭前是否尽量把发送缓冲区的数据发出去（kcp不保证送达）
function KcpSocket:close(flush)
    self.status = SocketMgr.CLOSING

    local s = self.s
    if not s then return end

    return s:stop(flush)
end

-- 获取当前连接的ip地址和端口（对端形态才有意义）
-- @return ip, port
function KcpSocket:address()
    local addr = self.addr
    if not addr then return nil end

    return (self.main_socket or self.s):get_udp_addr(addr)
end

function KcpSocket:start_event(fd, ev)
    if ev == SocketMgr.EV_READ then
        -- kcp的read事件说明这是服务端对端，fd其实是后端生成的vfd
        -- 原样送回backend，它靠 vfd>>32 找到所属的监听socket
        self.s:set_io_option("vfd", fd)

        fd = -1
    elseif ev == SocketMgr.EV_CONNECT then
        -- kcp没有connect事件，连上就可读（收发由kcp自己驱动）
        -- conv必须在KCP_ADD之前准备好，backend建ikcpcb要用
        self.s:set_io_option("conv", self.conv)

        ev = SocketMgr.EV_READ
    end

    return self.s:start(fd, LOCAL_ADDR, ev)
end

-- 监听socket连接
-- @param ip 监听的ip
-- @param port 监听的端口
-- @param boolean 返回是否成功
function KcpSocket:listen(ip, port)
    if not Socket.listen(self, ip, port) then
        return false
    end

    -- kcp没有链接过程，监听成功后就需要设置参数然后发送数据了
    return self:set_param()
end

-- 连接到其他服务器
-- @param host 目标服务器地址，可传域名或ip
-- @param port 目标服务器的端口
-- @param ip 目标服务器的ip，如果不传则从host解析
function KcpSocket:connect(host, port, ip)
    local conv = self.default_param.conv
    if not conv or 0 == conv then
        conv = math.random(1, 0x7FFFFFFF)
    end
    self.conv = conv

    -- conv在 start_event 里交给io，start由 Socket.connect 内部触发
    if not Socket.connect(self, host, port, ip) then
        return false
    end
    if not self:set_param() then
        return false
    end

    -- kcp没有握手，也不会有EV_CONNECT，连接建立即就绪
    -- 注意不能直接返回io_ready，它返回的是on_connected的结果(一般是nil)
    self:io_ready()

    return true
end

-- 发送数据的实际实现
-- @param ud string或者lightuserdata
-- @param size ud的长度
function KcpSocket:send_pkt(ud, size)
    if size > self.MAX_MSG then
        return error("kcp packet over max msg, size=" .. size ..
                     ", max=" .. self.MAX_MSG)
    end

    -- 走 send_srv（复用Packet机制），不给Socket加新方法
    return self.s:send_srv(ud, size)
end

return KcpSocket
