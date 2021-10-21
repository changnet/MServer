-- conn.lua
-- 2017-12-14
-- xzc

local util = require "engine.util"

---------------------------- 下面这些接口由底层回调 ------------------------------
----------------------- 直接回调到对应的连接对象以实现多态 ------------------------
__conn = __conn or {}

-- 因为gc的延迟问题，用以弱表可能会导致取所有连接时不准确
-- setmetatable(self.conn, {["__mode"]='v'})

local __conn = __conn

-- 接受新的连接
function conn_accept(conn_id, new_conn_id)
    local new_conn = __conn[conn_id]:conn_accept(new_conn_id)
    __conn[new_conn_id] = new_conn
end

-- 连接进行初始化
function conn_new(conn_id, ...)
    return __conn[conn_id]:conn_new(...)
end

-- io初始化成功
function io_ok(conn_id, ...)
    local conn = __conn[conn_id]

    conn.ok = true
    return __conn[conn_id]:io_ok(...)
end

-- 连接断开
function conn_del(conn_id)
    local conn = __conn[conn_id]
    __conn[conn_id] = nil

    conn.ok = false
    conn:on_disconnected()
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

    local param = self.default_param
    local conn_id = self.conn_id

    -- 读写方式，是否使用SSL
    if self.ssl then
        network_mgr:set_conn_io(conn_id, network_mgr.IOT_SSL, self.ssl)
    else
        network_mgr:set_conn_io(conn_id, network_mgr.IOT_NONE)
    end
    -- 编码方式，如bson、protobuf、flatbuffers等
    network_mgr:set_conn_codec(conn_id, param.cdt or network_mgr.CDT_NONE)
    -- 打包方式，如http、自定义的tcp打包、websocket打包
    network_mgr:set_conn_packet(conn_id, param.pkt or network_mgr.PT_NONE)

    local action = param.action or 1 -- over_action，1 表示缓冲区溢出后断开
    local chunk_size = param.chunk_size or 8192 -- 单个缓冲区大小
    local send_chunk_max = param.send_chunk_max or 1 -- 发送缓冲区数量
    local recv_chunk_max = param.recv_chunk_max or 1 -- 接收缓冲区数

    network_mgr:set_send_buffer_size(conn_id, send_chunk_max, chunk_size, action)
    network_mgr:set_recv_buffer_size(conn_id, recv_chunk_max, chunk_size)
end

-- 根据连接id获取对象
function Conn:get_conn(conn_id)
    return __conn[conn_id]
end

-- 把连接id和对象绑定
function Conn:set_conn(conn_id, conn)
    __conn[conn_id] = conn
end

-- 接受新连接
function Conn:conn_accept(new_conn_id)
    local mt = getmetatable(self) or self

    -- 取监听socket的元表来创建同类型的对象
    -- 并且把监听socket的ssl回调之类的复制到新socket
    local conn = mt(new_conn_id)

    conn.ssl = self.ssl

    -- 新建立的socket继承监听socket的参数
    -- 必须用rawget，避免取到元表的函数，那样会影响热更
    -- 如果需要逻辑里要覆盖这几个回调，那应该在table中覆盖而不是元表
    conn.on_cmd = rawget(self, "on_cmd")
    conn.on_accepted = rawget(self, "on_accepted")
    conn.on_connected = rawget(self, "on_connected")
    conn.on_disconnected = rawget(self, "on_disconnected")

    conn.default_param = rawget(self, "default_param")

    -- 必须在继承后设置参数，不然用些初始化的参数就会不对
    conn:set_conn_param()

    __conn[new_conn_id] = conn

    conn:on_accepted()
    return conn
end

-- 连接成功(或失败)
function Conn:conn_new(e)
    if 0 == e then
        self:set_conn_param()
    else
        self:on_disconnected(e)
    end
end

-- 连接断开
function Conn:on_disconnected(e)
end

-- 连接建立完成(包括SSL等握手完成)
function Conn:on_connected()
end

-- io初始化完成
function Conn:io_ok()
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
function Conn:connect(host, port)
    self.ip = util.get_addr_info(host)
    -- 这个host需要注意，对于http、ws，需要传域名而不是ip地址。这个会影响http头里的
    -- host字段
    -- 对www.example.com请求时，如果host为一个ip，是会返回404的
    self.host = host
    self.port = port

    self.conn_id = network_mgr:connect(
        self.ip, port, self.default_param.connect_type)

    self:set_conn(self.conn_id, self)

    return self.conn_id
end

-- 以https试连接到其他服务器
-- @param host 目标服务器地址
-- @param port 目标服务器端口
-- @param ssl 用new_ssl_ctx创建的ssl_ctx
function Conn:connect_s(host, port, ssl)
    self.ssl = assert(ssl)

    return self:connect(host, port)
end

-- 监听http连接
-- @param ip 监听的ip
-- @param port 监听的端口
function Conn:listen(ip, port)
    self.conn_id = network_mgr:listen(ip, port, self.default_param.listen_type)

    self:set_conn(self.conn_id, self)
    return true
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
    return network_mgr:close(self.conn_id, flush)
end

return Conn
