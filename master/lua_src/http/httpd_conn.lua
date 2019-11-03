-- httpd_conn.lua
--- 2017-12-16
-- xzc

-- 后台http连接

local HttpdConn = oo.class( ... )

function HttpdConn:__init( conn_id )
    self.conn_id = conn_id
end

function HttpdConn:conn_del()
    return g_httpd:conn_del( self.conn_id )
end

function HttpdConn:command_new( url,body )
    return g_httpd:do_command( self,url,body )
end

function HttpdConn:send_pkt( pkt )
    return network_mgr:send_raw_packet( self.conn_id,pkt )
end

function HttpdConn:close( flush )
    g_conn_mgr:set_conn( self.conn_id,nil )
    return network_mgr:close( self.conn_id,flush )
end

return HttpdConn
