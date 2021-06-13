-- conn_mgr.lua
-- 2017-12-14
-- xzc
-- 连接管理
local ConnMgr = oo.singleton(...)

function ConnMgr:__init()
    self.conn = {}
    -- setmetatable(self.conn, {["__mode"]='v'})
end

function ConnMgr:get_conn(conn_id)
    return self.conn[conn_id]
end

function ConnMgr:set_conn(conn_id, conn)
    self.conn[conn_id] = conn
end

local conn_mgr = ConnMgr()

---------------------------- 下面这些接口由底层回调 ------------------------------
----------------------- 直接回调到对应的连接对象以实现多态 ------------------------

-- 接受新的连接
function conn_accept(conn_id, new_conn_id)
    local new_conn = conn_mgr.conn[conn_id]:conn_accept(new_conn_id)
    conn_mgr.conn[new_conn_id] = new_conn
end

-- 连接进行初始化
function conn_new(conn_id, ...)
    return conn_mgr.conn[conn_id]:conn_new(...)
end

-- 连接成功
function conn_ok(conn_id, ...)
    return conn_mgr.conn[conn_id]:conn_ok(...)
end

-- 连接断开
function conn_del(conn_id)
    conn_mgr.conn[conn_id]:conn_del()
    conn_mgr.conn[conn_id] = nil
end

-- 消息回调,底层根据不同类，参数也不一样
function command_new(conn_id, ...)
    return conn_mgr.conn[conn_id]:command_new(...)
end

-- 转发的客户端消息
function css_command_new(conn_id, ...)
    return conn_mgr.conn[conn_id]:css_command_new(...)
end

-- rpc特殊处理，由rpc.lua自己分发

-- -- rpc请求
-- function rpc_command_new( conn_id,... )
--     return conn_mgr.conn[conn_id]:rpc_command_new( ... )
-- end

-- -- rpc返回
-- function rpc_command_return( conn_id,... )
--     return conn_mgr.conn[conn_id]:rpc_command_return( ... )
-- end

-- 握手
function handshake_new(conn_id, ...)
    return conn_mgr.conn[conn_id]:handshake_new(...)
end

-- 控制帧，比如websocket的ping、pong
function ctrl_new(conn_id, ...)
    return conn_mgr.conn[conn_id]:ctrl_new(...)
end

return conn_mgr
