-- srv_conn server connection

local g_command_mgr = g_command_mgr
local g_network_mgr = g_network_mgr

local network_mgr = network_mgr
local Srv_conn = oo.class( nil,... )

function Srv_conn:__init( conn_id )
    self.auth = false
    self.beat = 0
    self.fchk = 0 -- fail check
    self.session = 0
    self.conn_id = conn_id
end

-- 发送数据包
function Srv_conn:send_pkt( cmd,pkt,ecode )
    return network_mgr:send_s2s_packet( self.conn_id,cmd,ecode or 0,pkt )
end

-- 给客户端发送数据包 !!!当前连接必须是网关链接!!!
function Srv_conn:send_clt_pkt( pid,cmd,pkt,ecode )
    return network_mgr:send_ssc_packet(
        self.conn_id,pid,network_mgr.CDC_PROTOBUF,cmd,ecode or 0,pkt )
end

-- 发送数据包
function Srv_conn:send_rpc_pkt( unique_id,name,... )
    return network_mgr:send_rpc_packet( self.conn_id,cmd,ecode or 0,pkt )
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

-- 认证成功
function Srv_conn:authorized( session )
    self.auth = true
    self.session = session
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
