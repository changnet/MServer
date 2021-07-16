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

-- 连接成功
function conn_ok(conn_id, ...)
    return __conn[conn_id]:conn_ok(...)
end

-- 连接断开
function conn_del(conn_id)
    __conn[conn_id]:conn_del()
    __conn[conn_id] = nil
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
function Conn:set_conn_param(conn_id)
    -- 读写方式，是否使用SSL
    network_mgr:set_conn_io(conn_id, self.iot or network_mgr.IOT_NONE)
    -- 编码方式，如bson、protobuf、flatbuffers等
    network_mgr:set_conn_codec(conn_id, self.cdc or network_mgr.CT_PROTOBUF)
    -- 打包方式，如http、自定义的tcp打包、websocket打包
    network_mgr:set_conn_packet(conn_id, self.pkt or network_mgr.PT_NONE)


    local action = self.action or 1 -- over_action，1 表示缓冲区溢出后断开
    local chunk_size = self.chunk_size or 8192 -- 单个缓冲区大小
    local send_chunk_max = self.send_chunk_max or 128 -- 发送缓冲区数量
    local recv_chunk_max = self.recv_chunk_max or 8 -- 接收缓冲区数

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

return Conn
