-- conn.lua
-- 2017-12-14
-- xzc

local util = require "engine.util"
local Socket = require "engine.Socket"

---------------------------- 下面这些接口由底层回调 ------------------------------
----------------------- 直接回调到对应的连接对象以实现多态 ------------------------
__conn = __conn or {}
__conn_id_seed = __conn_id_seed or 0

-- 因为gc的延迟问题，用以弱表可能会导致取所有连接时不准确
-- setmetatable(self.conn, {["__mode"]='v'})

local __conn = __conn
local __conn_id_seed = __conn_id_seed

local function next_id()
    local id = __conn_id_seed + 1
    if __conn[id] then
        for _ = 1, 1000000 do
            id = id + 1
            if not __conn[id] then break end
        end
    end

    __conn_id_seed = id
    if __conn_id_seed > 0xFFFFFFFF then __conn_id_seed = 1 end

    return id
end

-- 在脚本执行重连后，conn对象在脚本被重用。但C++那边回调还需要旧对象来处理
local reconnect_conn =
{
    on_disconnected = function() end
}

-- 接受新的连接
function conn_accept(conn_id, fd)
    __conn[conn_id]:conn_accept(fd)
end

-- 连接进行初始化
function conn_new(conn_id, ...)
    return __conn[conn_id]:conn_new(...)
end

-- io初始化成功
function conn_io_ready(conn_id, ...)
    local conn = __conn[conn_id]

    conn.ok = true
    return __conn[conn_id]:io_ready(...)
end

-- 连接断开
function conn_del(conn_id, e)
    local conn = __conn[conn_id]
    __conn[conn_id] = nil

    conn.ok = false
    conn:on_disconnected(e)
end

-- 消息回调,底层根据不同类，参数也不一样
function command_new(conn_id, ...)
    return __conn[conn_id]:on_cmd(...)
end

-- 转发的客户端消息
function css_command_new(conn_id, ...)
    return __conn[conn_id]:css_command_new(...)
end

-- 握手(websocket用到这个)
function handshake_new(conn_id, ...)
    return __conn[conn_id]:handshake_new(...)
end

-- 控制帧，比如websocket的ping、pong
function ctrl_new(conn_id, ...)
    return __conn[conn_id]:ctrl_new(...)
end

--------------------------------------------------------------------------------
--------------------------------------------------------------------------------
--------------------------------------------------------------------------------

-- 网络连接基类
local Conn = oo.class(...)

function Conn:__init()
    local conn_id = next_id()

    self.s = Socket(conn_id)
    self.conn_id = conn_id
    __conn[conn_id] = self
end

-- on_cmd 收到消息时触发
-- on_accepted accept成功时触发
-- on_connected 连接建立完成(包括SSL、websocket握手完成)
-- on_disconnected 断开或者连接失败时触发

-- 设置io读写、编码、打包方式
function Conn:set_conn_param()
    --[[
        param = {
            listen_type = network_mgr.CT_SCCN, -- 监听的连接类型
            connect_type = network_mgr.CT_CSCN, -- 连接类型
            cdt = network_mgr.CDT_PROTOBUF, -- 编码类型
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
    s:set_packet(param.pkt)

    local action = param.action or 1 -- over_action，1 表示缓冲区溢出后断开
    local send_chunk_max = param.send_chunk_max or 1 -- 发送缓冲区数量
    local recv_chunk_max = param.recv_chunk_max or 1 -- 接收缓冲区数

    s:set_buffer_params(send_chunk_max, recv_chunk_max, action)
end

-- 根据连接id获取对象
function Conn:get_conn(conn_id)
    return __conn[conn_id]
end

-- 接受新连接
function Conn:conn_accept(fd)
    local mt = getmetatable(self) or self

    -- 取监听socket的元表来创建同类型的对象
    -- 并且把监听socket的ssl回调之类的复制到新socket
    local conn = mt()

    conn.ssl = self.ssl

    -- 新建立的socket继承监听socket的参数
    -- 必须用rawget，避免取到元表的函数，那样会影响热更
    -- 如果需要逻辑里要覆盖这几个回调，那应该在table中覆盖而不是元表
    conn.on_cmd = rawget(self, "on_cmd")
    conn.on_accepted = rawget(self, "on_accepted")
    conn.on_connected = rawget(self, "on_connected")
    conn.on_disconnected = rawget(self, "on_disconnected")

    conn.default_param = rawget(self, "default_param")

    if not conn.s:start(fd) then
        print("conn accept start fail", self.conn_id, fd)
        return
    end

    __conn[conn.conn_id] = conn

    -- 必须在继承后设置参数，不然用些初始化的参数就会不对
    conn:set_conn_param()

    -- 注意这个事件socket并未连接完成，不可发放数据，on_connected事件才完成
    conn:on_accepted()

    -- ssl 需要握手
    if conn.ssl then
        conn.s:io_init_accept()
    else
        conn:io_ready()
    end
end

-- 连接成功(或失败)
function Conn:conn_new(e)
    if 0 == e then
        self:set_conn_param()
        if self.ssl then
            self.s:io_init_connect()
        else
            self:io_ready()
        end
    else
        self:on_disconnected(e, true)
    end
end

-- 连接断开
-- @param e 连接断开错误码，对应errno或者WSGetLastError
-- @param is_conn 是否主动连接失败（这事件会触发两次）
function Conn:on_disconnected(e, is_conn)
end

-- 连接建立完成(包括SSL等握手完成)
function Conn:on_connected()
end

-- io初始化完成
function Conn:io_ready()
    -- 大部分socket在io(如SSL)初始化完成时整个连接就建立完成了
    -- 但像websocket这种，还需要进行一次websocket握手
    return self:on_connected()
end

-- 监听到新连接创建(还没握手完成)
function Conn:on_accepted()
end

-- 连接到其他服务器
-- @param host 目标服务器地址
-- @param port 目标服务器端口
-- @param ip 目标服务器的ip，如果不传从则host解析
function Conn:connect(host, port, ip)
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
function Conn:reconnect()
    assert(not self.ok)

    -- 如果当前socket未关闭，则关闭
    if __conn[self.conn_id] then
        network_mgr:close(self.conn_id)

        -- 旧的conn切换到一个没用的conn对象，C++回调时不影响当前对象，当前对象继续使用
        __conn[self.conn_id] = reconnect_conn
    end

    return self:connect(self.ip, self.port)
end

-- 以https试连接到其他服务器
-- @param host 目标服务器地址
-- @param port 目标服务器端口
-- @param ssl 用new_ssl_ctx创建的ssl_ctx
-- @param ip 目标服务器的ip，如果不传从则host解析
function Conn:connect_s(host, port, ssl, ip)
    self.ssl = assert(ssl)

    return self:connect(host, port, ip)
end

-- 监听http连接
-- @param ip 监听的ip
-- @param port 监听的端口
-- @param boolean, message 返回是否成功，失败后面带message
function Conn:listen(ip, port)
    self.listen_ip = ip
    self.listen_port = port

    local e = self.s:listen(ip, port)
    local ok = e >= 0

    self.ok = ok -- 对于监听的socket，不会触发io_ok，这里直接设置ok状态
    return ok
end

-- 以https方式监听http连接
-- @param ip 监听的ip
-- @param port 监听的端口
-- @param ssl 用new_ssl_ctx创建的ssl_ctx
function Conn:listen_s(ip, port, ssl)
    self.ssl = assert(ssl)
    return self:listen(ip, port)
end

-- 关闭链接
-- @param flush 关闭前是否发送缓冲区的数据
function Conn:close(flush)
    -- 关闭时会触发conn_del，在那边删除
    -- self:set_conn(self.conn_id, nil)
    return self.s:stop(self.conn_id, flush)
end

-- 获取当前连接的ip地址和端口
-- @return ip, port
function Conn:address()
    return self.s:address()
end

return Conn
