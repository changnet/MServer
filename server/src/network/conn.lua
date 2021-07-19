-- conn.lua
-- 2017-12-14
-- xzc


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
    conn:conn_del()
end

-- 消息回调,底层根据不同类，参数也不一样
function command_new(conn_id, ...)
    return __conn[conn_id]:command_new(...)
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

-- 设置io读写、编码、打包方式
function Conn:set_conn_param()
    --[[
        param = {
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

-- 连接断开
function Conn:conn_del()
end

-- 连接建立完成(包括SSL等握手完成)
function Conn:conn_ok()
end

-- io初始化完成
function Conn:io_ok()
    -- 大部分socket在io(如SSL)初始化完成时整个连接就建立完成了
    -- 但像websocket这种，还需要进行一次websocket握手
    return self:conn_ok()
end

return Conn
