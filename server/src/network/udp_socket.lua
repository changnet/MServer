-- udp_socket.lua
-- udp的socket封装

local util = require "engine.util"
local EngineSocket = require "engine.Socket"

local Socket = require "network.socket"

local EV_READ = SocketMgr.EV_READ
local OPENED = SocketMgr.OPENED

local ADDR = LOCAL_ADDR

-- 已connect的udp socket用这个地址发送
-- family_为AF_UNSPEC(0)，底层会用::send发到connect时设置的默认地址
local DEFAULT_ADDR = string.rep("\0", 20)

--[[
udp没有连接的概念，一个C++ Socket对应N个对端。这里用一个轻量的UdpConn表示
一个对端，它不继承Socket，不分配socket_id，也不进__socket_hash，close()只是
把自己从session表里移除。
]]
local UdpConn = oo.class("UdpConn")

function UdpConn:__init(parent, addr_key)
    self.parent = parent
    self.s = parent.s -- 与父socket共用同一个C++对象
    self.addr_key = addr_key -- 20字节二进制串，同时是session的key
    self.active = ev:time()
end

-- 发送数据
-- @param ud string或者lightuserdata(从on_message传进来的)
-- @param size ud的长度，ud为string时可不填
-- @return 是否成功，失败时带错误信息
function UdpConn:send_pkt(ud, size)
    return self.parent:send_udp_pkt(self.addr_key, ud, size)
end

-- 获取对端的ip和port
-- @return ip, port
function UdpConn:address()
    return self.s:get_udp_addr(self.addr_key)
end

-- 断开连接，注意这不是关闭fd，只是从session里移除
function UdpConn:close()
    self.parent:del_conn(self.addr_key)
end

-- 收到数据，业务层重写这个
function UdpConn:on_message(ud, size)
end

-- 新的对端接入（第一个包到达时触发）
function UdpConn:on_accepted()
end

-- 从session移除时触发
function UdpConn:on_disconnected()
end

--[[
udp的socket，服务端监听端口后用session表管理所有对端，客户端connect后只有一个对端。

服务端的消息回调写在UdpConn上：
    function MyConn:on_message(ud, size) ... end
    srv.conn_class = MyConn

客户端(connect出来的)直接重写on_message：
    clt.on_message = function(self, addr_key, ud, size) ... end
]]
local UdpSocket = oo.class("UdpSocket", Socket)

UdpSocket.MTU = 1200 -- 单个包的最大长度，C++不做限制，策略放这里
UdpSocket.conn_class = UdpConn
UdpSocket.session_timeout = 300 -- 对端多久没有数据则清理，0表示不清理

UdpSocket.default_param = {
    pkt = EngineSocket.PT_UDPSTREAM, -- 打包类型
    -- over_action，缓冲区溢出后的处理方式
    -- 一个udp socket对应所有对端，1(断开)会让一个对端的攻击搞挂整个服务，
    -- 因此默认用2(阻塞等待)，与客户端的连接不同
    action = 2,
    -- udp每个包有24字节的帧头(4字节长度 + 20字节地址)，小包场景开销较大
    send_byte_max = 4 * 1024 * 1024, -- 发送缓冲区数量
    recv_byte_max = 4 * 1024 * 1024 -- 接收缓冲区数
}

function UdpSocket:__init()
    Socket.__init(self)

    self.session = {}
end

-- udp用IOT_UDP(2)，不用ssl
function UdpSocket:set_io()
    local pio = self.s:set_io(2)
    assert(pio)
end

-- 把20字节的地址串解析成 ip:port，调试和日志用
-- @param addr_key 20字节的地址串
function UdpSocket:addr_tostring(addr_key)
    local ip, port = self.s:get_udp_addr(addr_key)
    return string.format("%s:%d", ip, port)
end

-- 监听udp端口
-- @param ip 监听的ip
-- @param port 监听的端口
-- @return 是否成功
function UdpSocket:listen(ip, port)
    self.listen_ip = ip
    self.listen_port = port

    -- 必须在启动之前设置好io、packet、缓冲区，不然backend线程会触发未初始化的读写
    self:set_param()
    self:set_ip_version(ip)

    local fd = self.s:udp_listen(ADDR, ip, port)
    if fd > 0 then
        self.status = OPENED
        self.s:set_watcher_event(EV_READ)
        return true
    end

    return false
end

-- 连接到其他服务器，udp的connect只是设置默认地址，不会真正连接
-- @param host 目标服务器地址
-- @param port 目标服务器端口
-- @param ip 目标服务器的ip，不传则从host解析
-- @return 是否成功
function UdpSocket:connect(host, port, ip)
    if not ip then ip = util.get_addr_info(host, 0 == self.ip_version) end

    self.ip = ip
    self.host = host
    self.port = port

    self:set_param()
    self:set_ip_version(ip)

    -- udp没有连接握手，也不需要等EV_CONNECT，这里直接标记连接完成
    local fd = self.s:udp_connect(ADDR, ip, port)
    if fd > 0 then
        self.status = OPENED
        self.s:set_watcher_event(EV_READ)
        self:on_connected()
        return true
    end

    return false
end

-- 发送数据，客户端(已connect)走这个接口
-- 服务端没有默认地址，必须通过UdpConn发送
-- @param ud string或者lightuserdata
-- @param size ud的长度，ud为string时可不填
-- @return 是否成功，失败时带错误信息
function UdpSocket:send_pkt(ud, size)
    if self:is_server() then
        eprint("udp server send_pkt without addr, use conn:send_pkt")
        return false, "no addr"
    end

    return self:send_udp_pkt(DEFAULT_ADDR, ud, size)
end

-- 发送数据的实际实现
-- @param addr_key 20字节的地址串
-- @param ud string或者lightuserdata
-- @param size ud的长度，ud为string时可不填
-- @return 是否成功，失败时带错误信息
function UdpSocket:send_udp_pkt(addr_key, ud, size)
    local len = type(ud) == "string" and #ud or size

    if not len or len < 0 then
        eprint("udp send_pkt: invalid size")
        return false, "invalid size"
    end
    if len > self.MTU then
        return false, "oversize"
    end

    return self.s:send_udp(addr_key, ud, len)
end

-- 收到一个udp数据包，这是C++的回调入口
-- @param addr_key 20字节的地址串
-- @param ud 数据的lightuserdata
-- @param size 数据长度(已扣除24字节帧头)
function UdpSocket:on_message(addr_key, ud, size)
    -- MTU策略放在lua，超长的包直接丢弃
    -- 不能返回PC_ERROR，否则一个恶意大包就会关掉整个udp服务
    if size and size > self.MTU then
        self.oversize = (self.oversize or 0) + 1
        eprint("udp packet oversize", self:addr_tostring(addr_key), size)
        return
    end

    local conn = self:get_conn(addr_key)
    if conn == self then
        -- 客户端只有一个对端，业务层直接重写on_message即可，不需要再分发
        return
    end

    return conn.on_message(conn, ud, size)
end

-- 根据地址获取对应的连接，没有则创建
-- @param addr_key 20字节的地址串
function UdpSocket:get_conn(addr_key)
    -- 客户端只有一个对端，不需要会话表
    if not self:is_server() then return self end

    local conn = self.session[addr_key]
    if not conn then
        conn = self.conn_class(self, addr_key)
        self.session[addr_key] = conn
        conn:on_accepted()
    end
    conn.active = ev:time()

    return conn
end

-- 删除一个连接，注意这不是关闭fd
function UdpSocket:del_conn(addr_key)
    local conn = self.session[addr_key]
    if not conn then return end

    self.session[addr_key] = nil
    conn:on_disconnected()
end

-- 清理超时的连接，需要业务层定时调用
-- @param now 当前时间，不传则用ev:time()
function UdpSocket:check_session(now)
    if 0 == self.session_timeout then return end

    now = now or ev:time()

    local timeout = self.session_timeout
    local session = self.session
    for addr_key, conn in pairs(session) do
        if now - conn.active > timeout then
            session[addr_key] = nil
            conn:on_disconnected()
        end
    end
end

-- 关闭时通知所有对端
function UdpSocket:on_disconnected()
    local session = self.session
    self.session = {}

    for _, conn in pairs(session) do
        conn:on_disconnected()
    end
end

return UdpSocket
