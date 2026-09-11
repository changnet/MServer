local EngineSocket = require "engine.Socket"
local Socket = require "network.socket"

local KcpSocket = oo.class("KcpSocket", Socket)

KcpSocket.MAX_MSG = 64 * 1024

KcpSocket.default_param = {
    pkt = EngineSocket.PT_KCPSTREAM, -- 打包类型
    io_type = EngineSocket.IOT_KCP, -- 读写方式
    -- over_action，缓冲区溢出后的处理方式
    -- kcp下每个对端是一条独立连接，缓冲区溢出只影响它自己，可以断开。
    -- ★ 溢出判定读的是这条连接自己的 EVIO 的 mask_（服务端对端是那个虚拟EVIO），
    --   而不是监听主EVIO的，所以一个暴力对端不会影响其他对端
    action = 1,
    send_byte_max = 1024 * 1024, -- 发送缓冲区数量
    recv_byte_max = 1024 * 1024, -- 接收缓冲区数量
    conv = 0, -- 0 表示随机，仅客户端形态使用
}

--[[
kcp有连接语义，但服务器的一个fd上会有多个对端。
    KcpSocket()                 独立的客户端(自己connect到一个对端)
    KcpSocket(addr, main, conv) 服务器上的一个对端，复用主socket的fd收发数据

与UdpSocket对端不同，kcp对端是"一条连接"：有自己的socket_id、EVIO、缓冲区
和溢出策略，必须进 SocketMgr（否则backend派发EV_READ时找不到对象）
]]
function KcpSocket:__init(addr, main_socket, conv)
    if main_socket then
        -- 服务器的一个对端：不创建底层socket，但它是"一条连接"
        self.main_socket = main_socket.s
        self.addr        = addr
        self.conv        = conv

        -- 建 socket_id + EngineSocket（不建fd、不start、不进epoll）
        Socket.init_virtual(self)

        -- 新连接继承监听socket的参数
        self.default_param = main_socket.default_param or KcpSocket.default_param
        self:auto_set_io()
        self:set_param()

        -- 把「对端」这条连接交给backend：建ikcpcb + 登记路由
        assert(self.s:start_kcp(LOCAL_ADDR, main_socket.socket_id,
                                main_socket.s:fd(), conv, addr))
    else
        Socket.__init(self)
    end
end

-- 关闭链接
-- @param flush 关闭前是否发送缓冲区的数据
function KcpSocket:close(flush)
    self.status = SocketMgr.CLOSING

    local s = self.s
    if not s then return end

    -- 和 UdpSocket:close 一字不差：Socket::stop 内部会自己递 KCP_DEL，
    -- backend 收到后释放 ikcpcb + 摘路由
    return s:stop(flush)
end

-- 获取当前连接的ip地址和端口（对端形态才有意义）
-- @return ip, port
function KcpSocket:address()
    local addr = self.addr
    if not addr then return nil end

    return (self.main_socket or self.s):get_udp_addr(addr)
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
    -- conv 由客户端随机，必须在 start_kcp 之前写入
    local conv = self.default_param.conv
    if not conv or 0 == conv then
        conv = math.random(1, 0x7FFFFFFF)
    end
    self.conv = conv
    self.s:set_kcp_conv(conv)

    if not Socket.connect(self, host, port, ip) then
        return false
    end
    if not self:set_param() then
        return false
    end

    -- 客户端形态：listen_id = 0，listen_fd = 自己的fd，addr不传（默认值AF_UNSPEC）
    return self.s:start_kcp(LOCAL_ADDR, 0, self.s:fd(), conv, nil)
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
