-- netmgr.lua
-- 2015-09-14
-- xzc

-- 网络事件管理

local Net_mgr = oo.class( nil,... )

-- 定义4个网络事件
local EV_ACCEPT     = 1
local EV_READ       = 2
local EV_DISCONNECT = 3
local EV_CONNECT    = 4

-- 构造函数
function Net_mgr:__init()
    self.conn = {}
    setmetatable( self.conn,{ __mode = "v" } )  --保证连接对象能被释放
end

-- push一个连接对象
function Net_mgr:push( conn )
    self.conn[conn.fd] = conn
end

-- 总的事件回调
function Net_mgr:event_cb( fd,event,... )
    local conn = self.conn[fd]
    if not conn then
        ELOG( "event_cb no connnect object found" )
        return
    end
    
    local cb = nil
    if EV_READ == event then
        cb = conn.on_read
    elseif EV_DISCONNECT == event then
        cb = conn.on_disconnect
    elseif EV_ACCEPT == event then
        cb = conn.on_accept
    else
        ELOG( "unknow net event on %d",fd )
        return
    end
    
    xpcall( cb,__G__TRACKBACK__,conn,... )
end

local net_mgr = Net_mgr()    -- 创建单例

return net_mgr
