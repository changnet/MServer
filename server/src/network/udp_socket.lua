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

function UdpSocket:__init(addr, main_socket)
    Socket.__init(self)

    -- 对端地址，发送数据时要用到
    self.addr = addr

    -- udp作为服务器时，只有一个监听的主socket，所有客户端发送数据都要根据这个来
    self.main_socket = main_socket
end

-- 关闭链接
-- @param flush 关闭前是否发送缓冲区的数据
function Socket:close(flush)
    self.status = SocketMgr.CLOSING

    if self.listen_ip then
        return self.s:stop(flush)
    end
end

-- 发送数据的实际实现
-- @param ud string或者lightuserdata
-- @param size ud的长度，ud为string时可不填
function UdpSocket:send_pkt(ud, size)
    -- udp超过mtu的包会被丢弃，要自己做分包
    if size > self.MTU then
        return error("udp packet over mtu, size=" .. size .. ", mtu=" .. self.MTU)
    end

    local sock = self.s or self.main_socket
    return sock:send_clt(self.addr, ud, size)
end

return UdpSocket
