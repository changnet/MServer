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

-- 底层accept回调
function sccn_accept_new( conn_id )
    local listen_conn = self.conn[listen_conn]


end

-- 处理服务器连接
function Network_mgr:on_srv_acception( conn )
    local conn_id = g_unique_id:conn_id()
    while self.srv_conn[conn_id] do
        conn_id = g_unique_id:conn_id()
    end

    local srv_conn = Srv_conn( conn,conn_id )
    self.srv_conn[conn_id] = srv_conn

    local fd = conn:file_description()
    PLOG( "accept server connection,fd:%d",fd )
end

function Network_mgr:on_clt_acception( conn )
    local conn_id = g_unique_id:conn_id()
    while self.clt_conn[conn_id] do
        conn_id = g_unique_id:conn_id()
    end

    local clt_conn = Clt_conn( conn,conn_id )
    self.clt_conn[conn_id] = clt_conn

    local fd = conn:file_description()
    PLOG( "accept client connection,fd:%d",fd )
end

-- 主动连接其他服务器
function Network_mgr:connect_srv( srvs )
    for _,srv in pairs( srvs ) do
        local conn_id = g_unique_id:conn_id()
        while self.srv_conn[conn_id] do
            conn_id = g_unique_id:conn_id()
        end

        local srv_conn = Srv_conn( nil,conn_id )
        srv_conn:connect( srv.ip,srv.port )

        self.srv_conn[conn_id] = srv_conn
        PLOG( "server connect to %s:%d",srv.ip,srv.port )
    end
end

-- 服务器断开
function Network_mgr:srv_disconnect( srv_conn )
    srv_conn:close()
    self.srv_conn[srv_conn.conn_id] = nil
    self.srv[srv_conn.session] = nil

    PLOG( "%s disconnect",srv_conn:conn_name() )
end

-- 客户端断开
function Network_mgr:clt_disconnect( clt_conn )
    local pid     = clt_conn.pid

    PLOG( "clt_disconnect,pid %d",pid or 0 )

    g_account_mgr:role_offline( clt_conn.conn_id )

    self:clt_close( clt_conn )
    if pid then
        -- 通知其他服务器玩家下线
        local pkt = { pid = pid }
        g_command_mgr:srv_broadcast( SS.PLAYER_OFFLINE,pkt )
    end

end

-- 主动关闭客户端连接(只关闭连接，不处理其他帐号下线逻辑)
function Network_mgr:clt_close( clt_conn )
    self.clt_conn[clt_conn.conn_id] = nil
    if pid then self.clt[clt_conn.pid] = nil end

    clt_conn:close()
end

-- 服务器连接回调
function Network_mgr:srv_connected( srv_conn,errno )
    if 0 ~= errno then
        srv_conn:close()
        self.srv_conn[srv_conn.conn_id] = nil
        self.srv[srv_conn.session] = nil
        PLOG( "server connect(%s:%d) fail:%s",
            srv_conn.ip,srv_conn.port,util.what_error( errno ) )

        return
    end

    PLOG( "server connect(%s:%d) establish",srv_conn.ip,srv_conn.port )
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
    return true
end

-- 发起认证
function Network_mgr:register_pkt( g_command_mgr )
    local pkt =
    {
        name    = Main.srvname,
        session = Main.session,
        timestamp = ev:time(),
    }
    pkt.auth = util.md5( SRV_KEY,pkt.timestamp,pkt.session )

    pkt.clt_cmd = g_command_mgr:clt_cmd()
    pkt.srv_cmd = g_command_mgr:srv_cmd()
    pkt.rpc_cmd = g_command_mgr:rpc_cmd()

    return pkt
end

-- 定时器回调
function Network_mgr:do_timer()
    local check_time = ev:time() - SRV_ALIVE_INTERVAL
    for conn_id,srv_conn in pairs( self.srv_conn ) do
        local ts = srv_conn:check( check_time )
        if ts > SRV_ALIVE_TIMES then
            -- timeout
            PLOG( "%s server timeout",srv_conn:conn_name() )
        elseif ts > 0 then
            g_command_mgr:srv_send( srv_conn,SS.SYS_BEAT,{response = true} )
        end
    end
end

-- 获取服务器连接
function Network_mgr:get_srv_conn( session )
    return self.srv[session]
end

-- 设置客户端连接
function Network_mgr:set_clt_conn( pid,clt_conn )
    assert( "player already have a conn",nil == self.clt[pid] )

    self.clt[pid] = clt_conn
end

-- 获取客户端连接
function Network_mgr:get_clt_conn( pid )
    return self.clt[pid]
end

-- 根据conn_id获取连接
function Network_mgr:get_conn( conn_id )
    return self.clt_conn[conn_id] or self.srv_conn[conn_id]
end

local network_mgr = Network_mgr()

return network_mgr
