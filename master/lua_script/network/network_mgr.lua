-- network_mgr 网络连接管理

local util          = require "util"
local Timer         = require "Timer"
local Srv_conn      = oo.refer( "network.srv_conn" )
local Clt_conn      = oo.refer( "network.clt_conn" )

local network_mgr = network_mgr
local Network_mgr = oo.singleton( nil,... )

function Network_mgr:__init()
    self.srv = {}  -- session为key，连接对象为value
    self.clt = {}  -- pid为key，连接对象为value

    self.srv_conn = {} -- conn_id为key，服务器连接对象为value
    self.clt_conn = {} -- conn_id为key，客户端连接对象为value

    self.srv_listen = nil -- 监听服务器连接
    self.clt_listen = nil -- 监听客户端连接

    self.timer = Timer()
    self.timer:set_self( self )
    self.timer:set_callback( self.do_timer )

    self.timer:start( 5,5 )
end

--  监听服务器连接
function Network_mgr:srv_listen( ip,port )
    self.srv_listen = network_mgr:listen( ip,port,network_mgr.CNT_SSCN )
    PLOG( "%s listen for server at %s:%d",Main.srvname,ip,port )

    return true
end

--  监听客户端连接
function Network_mgr:clt_listen( ip,port )
    self.clt_listen = network_mgr:listen( ip,port,network_mgr.CNT_SCCN )
    PLOG( "%s listen for client at %s:%d",Main.srvname,ip,port )

    return true
end

-- 监听http连接
function Network_mgr:http_listen( ip,port )
    self.http_listen = network_mgr:listen( ip,port,network_mgr.CNT_HTTP )
    PLOG( "%s listen for http at %s:%d",Main.srvname,ip,port )

    return true
end

-- 主动连接其他服务器
function Network_mgr:connect_srv( srvs )
    for _,srv in pairs( srvs ) do
        local conn_id = 
            network_mgr:connect( srv.ip,srv.port,network_mgr.CNT_SSCN )

        self.srv_conn[conn_id] = Srv_conn( conn_id )
        PLOG( "server connect to %s:%d",srv.ip,srv.port )
    end
end

-- ============================================================================

-- 主动关闭客户端连接(只关闭连接，不处理其他帐号下线逻辑)
function Network_mgr:clt_close( clt_conn )
    self.clt_conn[clt_conn.conn_id] = nil
    if clt_conn.pid then self.clt[clt_conn.pid] = nil end

    network_mgr:close( clt_conn.conn_id )
end

-- 服务器认证
function Network_mgr:srv_register( conn,pkt )
    local auth = util.md5( SRV_KEY,pkt.timestamp,pkt.session )
    if pkt.auth ~= auth then
        ELOG( "Network_mgr:srv_register fail,session %d",pkt.session )
        return false
    end

    local ty,index,srvid = g_unique_id:srv_session_parse( pkt.session )
    if srvid ~= tonumber(Main.srvid) then
        ELOG( "Network_mgr:srv_register srvid not match,expect %s,got %d",
            Main.srvid,srvid )
        return
    end

    if self.srv[pkt.session] then
        ELOG( "Network_mgr:srv_register session conflict:%d",pkt.session )
        return false
    end

    self.srv[pkt.session] = conn
    network_mgr:set_session( conn.conn_id,pkt.session )
    return true
end

-- 定时器回调
function Network_mgr:do_timer()
    -- local check_time = ev:time() - SRV_ALIVE_INTERVAL
    -- for conn_id,srv_conn in pairs( self.srv_conn ) do
    --     local ts = srv_conn:check( check_time )
    --     if ts > SRV_ALIVE_TIMES then
    --         -- timeout
    --         PLOG( "%s server timeout",srv_conn:conn_name() )
    --     elseif ts > 0 then
    --         g_command_mgr:srv_send( srv_conn,SS.SYS_BEAT,{response = true} )
    --     end
    -- end
end

-- 获取服务器连接
function Network_mgr:get_srv_conn( session )
    return self.srv[session]
end

-- 设置客户端连接
function Network_mgr:bind_role( pid,clt_conn )
    assert( "player already have a conn",nil == self.clt[pid] )

    clt_conn:bind_role( pid )
    self.clt[pid] = clt_conn
    network_mgr:set_owner( clt_conn.conn_id,pid )
end

-- 获取客户端连接
function Network_mgr:get_clt_conn( pid )
    return self.clt[pid]
end

-- 根据conn_id获取连接
function Network_mgr:get_conn( conn_id )
    return self.clt_conn[conn_id] or self.srv_conn[conn_id]
end

-- ============================================================================

local _network_mgr = Network_mgr()

-- 底层accept回调
function sscn_accept_new( conn_id )
    local conn = Srv_conn( conn_id )
    _network_mgr.srv_conn[conn_id] = conn

    PLOG( "accept server connection:%d",conn_id )
end

-- 底层accept回调
function sccn_accept_new( conn_id )
    local conn = Clt_conn( conn_id )
    _network_mgr.clt_conn[conn_id] = conn

    PLOG( "accept client connection:%d",conn_id )
end

-- 底层connect回调
function sscn_connect_new( conn_id,ecode )
    if 0 ~= ecode then
        PLOG( "server connect(%d) error:%s",conn_id,util.what_error( ecode ) )
        _network_mgr.srv_conn[conn_id] = nil
        return
    end

    local conn = _network_mgr.srv_conn[conn_id]
    PLOG( "server connect (%d) establish",conn_id)

    conn:send_pkt( SS.SYS_SYN,g_command_mgr:command_pkt() )
end

-- 底层数据包回调
function ss_command_new( conn_id,session,cmd,errno,pkt )
    local conn = _network_mgr.srv_conn[conn_id]
    g_command_mgr:srv_dispatch( conn,cmd,pkt )
end

-- 底层连接断开回调
function sscn_connect_del( conn_id )
    local conn = _network_mgr.srv_conn[conn_id]

    if conn.session then _network_mgr.srv[conn.session] = nil end
    _network_mgr.srv_conn[conn_id] = nil

    PLOG( "%s connect del",conn:conn_name() )
end

-- 客户端连接断开回调
function sccn_connect_del( conn_id )
    local conn = _network_mgr.clt_conn[conn_id]

    _network_mgr.clt_conn[conn_id] = nil

    g_account_mgr:role_offline( conn_id )

    if conn.pid then
        _network_mgr.clt[conn.pid] = nil
        local pkt = { pid = conn.pid }
        g_command_mgr:srv_broadcast( SS.PLAYER_OFFLINE,pkt )
    end

    PLOG( "client connect del:%d",conn_id )
end

-- 客户端数据包回调
function cs_command_new( conn_id,pid,cmd,... )
    local conn = _network_mgr.clt_conn[conn_id]
    g_command_mgr:clt_dispatch( conn,cmd,... )
end

-- 转发的客户端数据包
function css_command_new( conn_id,pid,cmd,... )
    local conn = _network_mgr.srv_conn[conn_id]
    g_command_mgr:clt_dispatch_ex( conn,pid,cmd,... )
end

function http_accept_new()
    print( "http_accept_new" )
end

function http_connect_new()
    print( "http_connect_new" )
end

function http_connect_del()
    print( "http_connect_del" )
end

-- http回调
function http_command_new( conn_id,url,body )
    print( "http command new",conn_id,url,body )
    network_mgr:send_http_packet( conn_id,"hello world http" )
    network_mgr:close( conn_id );
end

return _network_mgr
