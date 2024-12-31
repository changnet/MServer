-- socket.lua
-- 2017-12-14
-- xzc

local util = require "engine.util"
local EngineSocket = require "engine.Socket"

local EV_READ = SocketMgr.EV_READ

-- 网络连接基类
local Socket = oo.class(...)

function Socket:__init()
    local socket_id = SocketMgr.next_id()

    self.s = EngineSocket(socket_id)
    self.socket_id = socket_id

    SocketMgr.add(self)
end

-- on_message 收到消息时触发
-- on_accepted accept成功时触发
-- on_connected 连接建立完成(包括SSL、websocket握手完成)
-- on_disconnected 断开或者连接失败时触发

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

    local s = self.s
    local param = self.default_param

    -- 读写方式，是否使用SSL
    if self.ssl then
        s:set_io(1, self.ssl)
    else
        s:set_io(0)
    end

    -- 打包方式，如http、自定义的tcp打包、websocket打包
    local e = s:set_packet(param.pkt)
    assert(0 == e)

    local action = param.action or 1 -- over_action，1 表示缓冲区溢出后断开
    local send_chunk_max = param.send_chunk_max or 1 -- 发送缓冲区数量
    local recv_chunk_max = param.recv_chunk_max or 1 -- 接收缓冲区数

    s:set_buffer_params(send_chunk_max, recv_chunk_max, action)
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
    else
        self:on_disconnected(e, true)
    end
end

-- 连接断开(主动断开、对方断开都会触发此事件)
-- @param e 连接断开错误码，对应errno或者WSGetLastError
function Socket:on_disconnected(e)
end

-- 连接建立完成(包括SSL等握手完成)
function Socket:on_connected()
end

-- io初始化完成
function Socket:io_ready()
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

    local e = self.s:connect(ip, port)

    return e >= 0
end

-- 重新连接，之前必须调用过connect函数
function Socket:reconnect()
    assert(false, "to be implement!!!")
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

    local e = self.s:listen(ip, port)
    local ok = e >= 0

    self.ok = ok -- 对于监听的socket，不会触io_ready，这里直接设置ok状态
    return ok
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
    -- 关闭时会触发conn_del，在那边删除
    -- self:set_conn(self.socket_id, nil)
    return self.s:stop(flush)
end

-- 获取当前连接的ip地址和端口
-- @return ip, port
function Socket:address()
    return self.s:address()
end

return Socket
