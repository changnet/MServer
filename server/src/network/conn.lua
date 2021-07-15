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
