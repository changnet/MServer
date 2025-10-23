-- socket.lua
-- 2017-12-14
-- xzc

local util = require "engine.util"
local EngineIO = require "engine.IO"
local EngineSocket = require "engine.Socket"

local EV_READ = SocketMgr.EV_READ
local OPENED = SocketMgr.OPENED
local OPENING = SocketMgr.OPENING
local CLOSING = SocketMgr.CLOSING

local ADDR = LOCAL_ADDR

-- 网络连接基类
local Socket = oo.class()

--[[
on_message
on_accepted
on_connected
on_disconnected

这几个接口作为业务接口，是可以在业务逻辑中重写的。因此，一些底层的逻辑不要放在里面
例如：self.status = xxx

而其他的，像on_connecting这种不是业务逻辑，可以放底层逻辑
]]

function Socket:__init()
    local socket_id = SocketMgr.next_id()

    self.s = EngineSocket(socket_id)
    self.socket_id = socket_id

    SocketMgr.add(self)
end

-- 读写方式，是否使用SSL
function Socket:set_io()
    if self.ssl then
        local pio = self.s:set_io(1, self.ssl)
        assert(pio)

        local sni = self.sni or self.host
        if sni then EngineIO.set_ssl_sni(pio, sni) end
        local cert_host = self.cert_host or self.host
        if cert_host and cert_host ~= "" then
            EngineIO.set_ssl_cert_host(pio, cert_host)
        end
        local mode = self.verify_mode
        if mode then EngineIO.set_ssl_verify_mode(pio, mode) end
    else
        local pio = self.s:set_io(0)
        assert(pio)
    end
end

-- 设置io读写、编码、打包方式
function Socket:set_param()
    --[[
        param = {
            pkt = network_mgr.PT_NONE, -- 打包类型
            action = 1, -- over_action，1 表示缓冲区溢出后断开
            chunk_size = 8192, -- 单个缓冲区大小
            send_chunk_max = 128, -- 发送缓冲区数量
            recv_chunk_max = 8 -- 接收缓冲区数
        }
    ]]


    self:set_io()
    local param = self.default_param

    -- 打包方式，如http、自定义的tcp打包、websocket打包
    local e = self.s:set_packet(param.pkt)
    assert(0 == e)

    local action = param.action or 1 -- over_action，1 表示缓冲区溢出后断开
    local send_chunk_max = param.send_chunk_max or 1 -- 发送缓冲区数量
    local recv_chunk_max = param.recv_chunk_max or 1 -- 接收缓冲区数

    self.s:set_buffer_params(send_chunk_max, recv_chunk_max, action)
end

-- 接受新连接
function Socket:on_accepting(fd)
    local mt = getmetatable(self) or self

    -- 取监听socket的元表来创建同类型的对象
    -- 并且把监听socket的ssl回调之类的复制到新socket
    local socket = mt()

    socket.ssl = self.ssl

    -- 新建立的socket继承监听socket的参数
    -- 必须用rawget，避免取到元表的函数，那样会影响热更
    -- 如果需要逻辑里要覆盖这几个回调，那应该在table中覆盖而不是元表
    socket.on_message = rawget(self, "on_message")
    socket.on_accepted = rawget(self, "on_accepted")
    socket.on_connected = rawget(self, "on_connected")
    socket.on_disconnected = rawget(self, "on_disconnected")

    socket.default_param = rawget(self, "default_param")

    if not socket.s:start(LOCAL_ADDR, fd, EV_READ) then
        print("socket accept start fail", self.socket_id, fd)
        return
    end

    SocketMgr.add(socket)

    -- 必须在继承后设置参数，不然用些初始化的参数就会不对
    socket:set_param()

    -- 注意这个事件socket并未连接完成，不可发放数据，on_connected事件才完成
    socket:on_accepted()

    -- ssl 需要握手
    if socket.ssl then
        socket.s:io_init_accept()
    else
        socket:io_ready()
    end
end

-- 处理连接回调事件(连接可能失败，on_connected是连接成功)
function Socket:on_connecting(e)
    if 0 == e then
        self:set_param()
        if self.ssl then
            self.s:io_init_connect()
        else
            self:io_ready()
        end
    end
end

-- 连接断开(主动断开、对方断开都会触发此事件)
function Socket:on_disconnected()
    -- self.status不为closing，则为对方关闭链接
    -- self.status = OPENING则为连接失败
    -- 可通过self.s:get_errno()获取错误码判断是否出现错误
end

-- 连接建立完成(包括SSL等握手完成)
function Socket:on_connected()

end

function Socket:set_option()
end

-- io初始化完成
function Socket:io_ready()
    self:set_option()
    self.status = OPENED
    self.s:set_watcher_event(SocketMgr.EV_READ)

    -- 大部分socket在io(如SSL)初始化完成时整个连接就建立完成了
    -- 但像websocket这种，还需要进行一次websocket握手
    return self:on_connected()
end

-- 监听到新连接创建(还没握手完成)
function Socket:on_accepted()
end

-- 连接到其他服务器
-- @param host 目标服务器地址
-- @param port 目标服务器端口
-- @param ip 目标服务器的ip，如果不传从则host解析
function Socket:connect(host, port, ip)
    if not ip then ip = util.get_addr_info(host) end
    -- 这个host需要注意，对于http、ws，需要传域名而不是ip地址
    -- 这个会影响http头里的host字段
    -- 对www.example.com请求时，如果host为一个ip，是会返回404的
    self.ip = ip
    self.host = host
    self.port = port

    local fd = self.s:connect(ADDR, ip, port)

    self.status = OPENING
    return fd > 0
end

-- 重新连接，之前必须调用过connect函数，并且完全关闭
function Socket:reconnect()
    -- 1. 之前必须调用过connect，有ip和port
    -- 2. socket必须完全关闭，包括底层的fd已完全关闭，否则会出现错误的消息回调

    local ip = self.ip
    if not ip then
        eprint("socket no ip found")
        return
    end
    if self.status ~= SocketMgr.CLOSED then
        eprint("socket no closed")
        return
    end
    if -1 ~= self.s:fd()  then
        eprint("socket fd closed")
        return
    end

    -- 底层socket包含了一些事件和标记，还有io和packet指针，缓冲区里也可能有未完成的数据
    -- 所以直接创建一个新的比较简单，重连在后端应该是频率比较少的操作
    -- socket_id是唯一标识，很多地方有用，所以必须用旧的
    self.s = EngineSocket(self.socket_id)
    SocketMgr.add(self)

    local fd = self.s:connect(ADDR, ip, self.port)

    self.status = OPENING
    return fd > 0
end

-- 以https试连接到其他服务器
-- @param host 目标服务器地址
-- @param port 目标服务器端口
-- @param ssl 用new_ssl_ctx创建的ssl_ctx
-- @param ip 目标服务器的ip，如果不传从则host解析
function Socket:connect_s(host, port, ssl, ip)
    self.ssl = assert(ssl)

    return self:connect(host, port, ip)
end

-- 监听http连接
-- @param ip 监听的ip
-- @param port 监听的端口
-- @param boolean, message 返回是否成功，失败后面带message
function Socket:listen(ip, port)
    self.listen_ip = ip
    self.listen_port = port

    local fd = self.s:listen(ADDR, ip, port)
    if fd > 0 then
        self.status = OPENED -- 对于监听的socket，不会触io_ready，这里直接设置状态
        return true
    end

    return false
end

-- 以https方式监听http连接
-- @param ip 监听的ip
-- @param port 监听的端口
-- @param ssl 用new_ssl_ctx创建的ssl_ctx
function Socket:listen_s(ip, port, ssl)
    self.ssl = assert(ssl)
    return self:listen(ip, port)
end

-- 关闭链接
-- @param flush 关闭前是否发送缓冲区的数据
function Socket:close(flush)
    self.status = CLOSING
    return self.s:stop(flush)
end

-- 获取当前连接的ip地址和端口
-- @return ip, port
function Socket:address()
    return self.s:address()
end

-- 当前socket是否为server模式(监听端为server，connect的为client)
function Socket:is_server()
    -- 如果主动connect，那肯定有host
    return not self.host
end

-- 获取错误码及错误原因
function Socket:get_error()
    local e = self.s:get_errno()
    local str = util.what_error(e)
    return e, str
end

return Socket
