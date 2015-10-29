-- netmgr.lua
-- 2015-09-14
-- xzc

-- 网络事件管理

local Net_mgr = oo.singleton( nil,... )

-- 构造函数
function Net_mgr:__init()
    ev:set_net_ref( self,self.accept_event,self.read_event,
        self.disconnect_event,self.connected_event )
    self.conn = {}
end

-- push一个连接对象
function Net_mgr:push( conn )
    self.conn[conn.fd] = conn
end

-- pop 一个连接对象
function Net_mgr:pop( conn )
    self.conn[conn.fd] = nil
end

-- accept事件
function Net_mgr:accept_event( fd,... )
    local conn = self.conn[fd]
    if not conn then
        ELOG( "accept no connnect object found" )
        return
    end

    xpcall( conn.on_accept,__G__TRACKBACK__,conn,... )
    return conn.sk
end

-- read事件
function Net_mgr:read_event( fd,... )
    local conn = self.conn[fd]

    xpcall( conn.on_read,__G__TRACKBACK__,conn,... )
end

-- disconnect事件
function Net_mgr:disconnect_event( fd,... )
    local conn = self.conn[fd]

    xpcall( conn.on_disconnect,__G__TRACKBACK__,conn,... )
end

-- connected事件
function Net_mgr:connected_event( fd,... )
    local conn = self.conn[fd]
    
    xpcall( conn.on_connected,__G__TRACKBACK__,conn,... )
end

local net_mgr = Net_mgr()    -- 创建单例

return net_mgr
