-- 客户端网络连接

local network_mgr = network_mgr
local Clt_conn = oo.class( nil,... )

function Clt_conn:__init( conn_id )
    self.auth = false
    self.beat = 0
    self.fchk = 0 -- fail check

    self.conn_id = conn_id
end

-- 发送数据包
function Clt_conn:send_pkt( cmd,pkt,errno )
    return network_mgr:send_clt_packet( self.conn_id,cmd,errno or 0,pkt )
end

-- 认证成功
function Clt_conn:authorized()
    self.auth = true
end

-- 将该链接绑定一个角色
function Clt_conn:bind_role( pid )
    self.pid = pid
end

-- 监听客户端连接
function Clt_conn:listen( ip,port )
    self.conn_id = network_mgr:listen( ip,port,network_mgr.CNT_SCCN )

    g_conn_mgr:set_conn( self.conn_id,self )
end

-- 连接断开
function Clt_conn:conn_del()
    return g_network_mgr:clt_conn_del( self.conn_id )
end

-- 消息回调
function Clt_conn:command_new( pid,cmd,... )
    return g_command_mgr:clt_dispatch( self,cmd,... )
end

-- 主动关闭连接
function Clt_conn:close()
    return network_mgr:close( self.conn_id )
end

-- 接受新客户端连接
function Clt_conn:conn_accept( new_conn_id )
    network_mgr:set_conn_io( new_conn_id,network_mgr.IOT_NONE )
    network_mgr:set_conn_codec( new_conn_id,network_mgr.CDC_PROTOBUF )
    network_mgr:set_conn_packet( new_conn_id,network_mgr.PKT_STREAM )

    local new_conn = Clt_conn( new_conn_id )
    g_network_mgr:clt_conn_accept( new_conn_id,new_conn )

    return new_conn
end

return Clt_conn
