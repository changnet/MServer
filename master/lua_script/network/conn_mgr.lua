-- conn_mgr.lua
-- 2017-12-14
-- xzc

-- 连接管理

local Conn_mgr = oo.singleton( nil,... )

function Conn_mgr:__init()
    self.conn = {}
end

function Conn_mgr:get_conn( conn_id )
    return self.conn[conn_id]
end

function Conn_mgr:set_conn( conn_id,conn )
    self.conn[conn_id] = conn
end

local conn_mgr = Conn_mgr()

---------------------------- 下面这些接口由底层回调 ------------------------------

-- 接受新的连接
function conn_accept( conn_id,new_conn_id )
    return self.conn[conn_id]:conn_accept( new_conn_id )
end

-- 连接成功
function conn_new( conn_id )
    return self.conn[conn_id]:conn_new()
end

-- 连接断开
function conn_del( conn_id )
    return self.conn[conn_id]:conn_del()
end

-- 消息回调,底层根据不同类，参数也不一样
function command_new( conn_id,... )
    return self.conn[conn_id]:command_new( ... )
end

-- 转发的客户端消息
function css_command_new( conn_id,... )
    return self.conn[conn_id]:css_command_new( ... )
end

-- rpc请求
function rpc_command_new( conn_id,... )
    return self.conn[conn_id]:rpc_command_new( ... )
end

-- rpc返回
function rpc_command_return( conn_id,... )
    return self.conn[conn_id]:rpc_command_return( ... )
end

return conn_mgr
