-- 客户端网络连接

local g_command_mgr = g_command_mgr
local g_network_mgr = g_network_mgr

local Clt_conn = oo.class( nil,... )

function Clt_conn:__init( conn_id )
    self.auth = false
    self.beat = 0
    self.fchk = 0 -- fail check

    self.conn_id = conn_id
end

-- 连接断开处理
function Clt_conn:on_disconnected()
    return g_network_mgr:clt_disconnect( self )
end

-- 认证成功
function Clt_conn:authorized( pkt )
    self.auth = true
    self.conn:set_on_command( self.on_command )
end

-- 关闭连接(TODO: 暂时用设置nil方式释放引用)
function Clt_conn:close()
    self.pid  = nil
    self.conn = nil
end

-- 将该链接绑定一个角色
function Clt_conn:bind_role( pid )
    self.pid = pid
end
