local EngineSocket = require "engine.Socket"

local Socket = require "network.socket"

local UdpSocket = oo.class("UdpSocket", Socket)

UdpSocket.MTU = 1200 -- 单个包的最大长度，C++不做限制，策略放这里

UdpSocket.default_param = {
    pkt = EngineSocket.PT_UDPSTREAM, -- 打包类型
    io_type = EngineSocket.IOT_UDP, -- 读写方式
    -- over_action，缓冲区溢出后的处理方式
    -- 一个udp socket对应所有对端，1(断开)会让一个对端的攻击搞挂整个服务，
    -- 因此默认用2(阻塞等待)，与客户端的连接不同
    action = 2,
    -- udp每个包有24字节的帧头(4字节长度 + 20字节地址)，小包场景开销较大
    send_byte_max = 4 * 1024 * 1024, -- 发送缓冲区数量
    recv_byte_max = 4 * 1024 * 1024 -- 接收缓冲区数
}

--[[
udp没有连接的概念，这里用一个UdpSocket表示一个"对端"。
    UdpSocket()                独立的客户端(自己connect到一个对端)
    UdpSocket(addr, main_sock) 服务器上的一个对端，复用主socket收发数据
]]
function UdpSocket:__init(addr, main_socket)
    if main_socket then
        -- 服务器的一个对端：不创建底层连接，也不注册到SocketMgr，
        -- 它只是主socket的一个"会话"，所有数据都走主socket
        self.main_socket = main_socket.s
        self.socket_id   = SocketMgr.next_id()
        self.addr = addr -- 对端地址，发送数据时要用到
    else
        Socket.__init(self)
    end
end

-- 关闭链接
-- @param flush 关闭前是否发送缓冲区的数据
function UdpSocket:close(flush)
    self.status = SocketMgr.CLOSING

    -- 服务器的对端没有自己的底层socket，它是复用主socket的
    local s = self.s
    if not s then return end

    return s:stop(flush)
end

-- 获取当前连接的ip地址和端口
-- @return ip, port
function UdpSocket:address()
    local addr = self.addr
    if not addr then return nil end

    -- get_udp_addr是C++侧注册的成员函数，用冒号调用(self已在索引1)
    return (self.main_socket or self.s):get_udp_addr(addr)
end

-- 监听socket连接
-- @param ip 监听的ip
-- @param port 监听的端口
-- @param boolean 返回是否成功
function UdpSocket:listen(ip, port)
    if not Socket.listen(self, ip, port) then
        return false
    end

    -- udp没有链接过程，监听成功后就需要设置参数然后发送数据了
    -- 现在udp不会触发连接成功之类的回调，直接判断listen、connect返回的值即可
    return self:set_param()
end

-- 连接到其他服务器
-- @param host 目标服务器地址，可传域名或ip
-- @param port 目标服务器的端口
-- @param ip 目标服务器的ip，如果不传从则host解析
function UdpSocket:connect(host, port, ip)
    if not Socket.connect(self, host, port, ip) then
        return false
    end

    -- udp没有链接过程，监听成功后就需要设置参数然后发送数据了
    -- 现在udp不会触发连接成功之类的回调，直接判断listen、connect返回的值即可
    return self:set_param()
end

-- 发送数据的实际实现
-- @param ud string或者lightuserdata
-- @param size ud的长度
function UdpSocket:send_pkt(ud, size)
    -- udp超过mtu的包会被丢弃，要自己做分包
    if size > self.MTU then
        return error("udp packet over mtu, size=" .. size .. ", mtu=" ..
                     self.MTU)
    end

    -- 服务器的对端要复用主socket。注意 __init 里才是给自己建 socket 的，
    -- 这里必须先判断 main_socket
    local sock = self.main_socket or self.s
    return sock:send_clt(self.addr, ud, size)
end

return UdpSocket
