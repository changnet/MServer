-- srv_conn server connection

local g_command_mgr = g_command_mgr
local g_network_mgr = g_network_mgr

local Srv_conn = oo.class( nil,... )

function Srv_conn:__init( conn_id )
    self.auth = false
    self.beat = 0
    self.fchk = 0 -- fail check
    self.conn_id = conn_id
end

-- timeout check
function Srv_conn:check( check_time )
    if self.beat < check_time then
        self.fchk = self.fchk + 1

        return self.fchk
    end

    self.fchk = 0
    return 0
end

-- close
function Srv_conn:close()
    -- self.conn:kill() -- if socket still active
    self.conn = nil -- release ref,so socket can be gc
end

-- 主动发起链接
function Srv_conn:connect( ip,port )
    self.ip   = ip
    self.port = port

    return self.conn:connect( ip,port )
end


-- 底层消息回调
function Srv_conn:on_command()
    self.beat = ev:time() -- 更新心跳

    local cmd,pid = self.conn:srv_next()
    while cmd do
        xpcall( g_command_mgr.srv_dispatcher,
            __G__TRACKBACK__, g_command_mgr, cmd, pid, self )

        cmd,pid = self.conn:srv_next( cmd )
    end
end

-- 断开回调
function Srv_conn:on_disconnected()
    return g_network_mgr:srv_disconnect( self )
end

-- connect回调
function Srv_conn:on_connected( errno )
    g_network_mgr:srv_connected( self,errno )

    if 0 ~=  errno then return end

    local pkt = g_network_mgr:register_pkt( g_command_mgr )
    g_command_mgr:srv_send( self,SS.SYS_SYN,pkt )
end

-- 认证成功
function Srv_conn:authorized( session )
    self.auth = true
    self.session = session
    self.conn:set_on_command( self.on_command )
end

-- 获取该连接名称
function Srv_conn:conn_name( session )
    -- 该服务器连接未经过认证
    if 0 == session then return "unauthorized" end

    local ty,index,srvid = 
        g_unique_id:srv_session_parse( session or self.session )

    local name = ""
    for _name,_ty in pairs( SRV_NAME ) do
        if ty == _ty then
            name = _name
            break
        end
    end

    return string.format( "%s(I%d.S%d)",name,index,srvid )
end

return Srv_conn
