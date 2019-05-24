-- srv_conn server connection

local network_mgr = network_mgr
local Srv_conn = oo.class( ... )

function Srv_conn:__init( conn_id )
    self.auth = false
    self.beat = 0
    self.fchk = 0 -- fail check
    self.session = 0
    self.conn_id = conn_id
    self.auto_conn = false -- 是否自动重连
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
function Srv_conn:send_rpc_pkt( unique_id,method_name,... )
    return network_mgr:send_rpc_packet( self.conn_id,unique_id,method_name,... )
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
function Srv_conn:authorized( pkt )
    self.auth = true
    self.name = pkt.name
    self.session = pkt.session
end

-- 获取基本类型名字(gateway、world)
function Srv_conn:base_name()
    return self.name
end

-- 获取该连接名称
function Srv_conn:conn_name( session )
    -- 该服务器连接未经过认证
    if 0 == session then return "unauthorized" end

    local ty,index,srvid =
        g_app:srv_session_parse( session or self.session )

    return string.format( "%s(I%d.S%d)",self.name,index,srvid )
end

-- 监听服务器连接
function Srv_conn:listen( ip,port )
    self.conn_id = network_mgr:listen( ip,port,network_mgr.CNT_SSCN )

    g_conn_mgr:set_conn( self.conn_id,self )
end

function Srv_conn:raw_connect()
    self.conn_ok = false
    self.conn_id = network_mgr:connect( self.ip,self.port,network_mgr.CNT_SSCN )

    g_conn_mgr:set_conn( self.conn_id,self )

    return self.conn_id
end

-- 连接到其他服务器
function Srv_conn:connect( ip,port )
    self.ip = ip
    self.port = port

    return self:raw_connect()
end

-- 重新连接
function Srv_conn:reconnect()
    self.auth = false
    self.session = 0

    return self:raw_connect()
end

-- 接受新的连接
function Srv_conn:conn_accept( new_conn_id )
    network_mgr:set_conn_io( new_conn_id,network_mgr.IOT_NONE )
    network_mgr:set_conn_codec( new_conn_id,network_mgr.CDC_PROTOBUF )
    network_mgr:set_conn_packet( new_conn_id,network_mgr.PKT_STREAM )

    -- 设置服务器之间链接缓冲区大小：16777216 = 16MB
    network_mgr:set_send_buffer_size( new_conn_id,16777216*4,16777216*4 )
    network_mgr:set_recv_buffer_size( new_conn_id,16777216*4,16777216*4 )

    local new_conn = Srv_conn( new_conn_id )
    g_network_mgr:srv_conn_accept( new_conn_id,new_conn )

    return new_conn
end

-- 连接成功
function Srv_conn:conn_new( ecode )
    if 0 == ecode then
        self.conn_ok = true
        network_mgr:set_conn_io( self.conn_id,network_mgr.IOT_NONE )
        network_mgr:set_conn_codec( self.conn_id,network_mgr.CDC_PROTOBUF )
        network_mgr:set_conn_packet( self.conn_id,network_mgr.PKT_STREAM )

        -- 设置服务器之间链接缓冲区大小：16777216 = 16MB
        network_mgr:set_send_buffer_size( self.conn_id,16777216*4,16777216*4 )
        network_mgr:set_recv_buffer_size( self.conn_id,16777216*4,16777216*4 )
    end

    return g_network_mgr:srv_conn_new( self.conn_id,ecode )
end

-- 连接断开
function Srv_conn:conn_del()
    self.conn_ok = false
    g_conn_mgr:set_conn( self.conn_id,nil )
    return g_network_mgr:srv_conn_del( self.conn_id )
end

-- 服务器之间消息回调
function Srv_conn:command_new( session,cmd,errno,pkt )
    self.beat = ev:time()
    return g_command_mgr:srv_dispatch( self,cmd,pkt )
end

-- 转发的客户端消息
function Srv_conn:css_command_new( pid,cmd,... )
    return g_command_mgr:clt_dispatch_ex( self,pid,cmd,... )
end

-- 主动关闭连接
function Srv_conn:close()
    self.conn_ok = false
    return network_mgr:close( self.conn_id )
end

-- 发送认证数据
function Srv_conn:send_register()
    local pkt =
    {
        name    = g_app.srvname,
        session = g_app.session,
        timestamp = ev:time(),
    }
    pkt.auth = util.md5( SRV_KEY,pkt.timestamp,pkt.session )
    self:send_pkt( SS.SYS_REG,pkt )
end

return Srv_conn
