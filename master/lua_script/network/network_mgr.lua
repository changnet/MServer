-- network_mgr 网络连接管理

local util          = require "util"
local Timer         = require "Timer"
local Srv_conn      = require "network.srv_conn"
local Clt_conn      = require "network.clt_conn"

local network_mgr = network_mgr
local Network_mgr = oo.singleton( nil,... )

local gateway_session = g_app:srv_session( 
    "gateway",tonumber(g_app.srvindex),tonumber(g_app.srvid) )

function Network_mgr:__init()
    self.srv = {}  -- session为key，连接对象为value
    self.clt = {}  -- pid为key，连接对象为value

    self.srv_conn = {} -- conn_id为key，服务器连接对象为value
    self.clt_conn = {} -- conn_id为key，客户端连接对象为value

    self.srv_listen_conn = nil -- 监听服务器连接
    self.clt_listen_conn = nil -- 监听客户端连接

    self.name_srv   = {}  -- 进程名为key，session为value

    self.srv_waiting = {} -- 等待重连的服务器连接

    local index = tonumber( g_app.srvindex )
    local srvid = tonumber( g_app.srvid )
    for name,_ in pairs( SRV_NAME ) do
        self.name_srv[name] = g_app:srv_session( name,index,srvid )
    end

    self.timer = g_timer_mgr:new_timer( self,10,5 )

    g_timer_mgr:start_timer( self.timer )
end

--  监听服务器连接
function Network_mgr:srv_listen( ip,port )
    self.srv_listen_conn = Srv_conn()
    self.srv_listen_conn:listen( ip,port )

    PLOG( "%s listen for server at %s:%d",g_app.srvname,ip,port )
    return true
end

--  监听客户端连接
function Network_mgr:clt_listen( ip,port )
    self.clt_listen_conn = Clt_conn()
    self.clt_listen_conn:listen( ip,port )

    PLOG( "%s listen for client at %s:%d",g_app.srvname,ip,port )
    return true
end

-- 主动连接其他服务器
function Network_mgr:connect_srv( srvs )
    for _,srv in pairs( srvs ) do
        local conn = Srv_conn()

        conn.auto_conn = true
        local conn_id = conn:connect( srv.ip,srv.port )

        self.srv_conn[conn_id] = conn
        PLOG( "server connect to %s:%d",srv.ip,srv.port )
    end
end

-- 重新连接到其他服务器
function Network_mgr:reconnect_srv( conn )
    local conn_id = conn:reconnect()

    self.srv_conn[conn_id] = conn
    PLOG( "server reconnect to %s:%d",conn.ip,conn.port )
end

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

    local ty,index,srvid = g_app:srv_session_parse( pkt.session )
    if srvid ~= tonumber(g_app.srvid) then
        ELOG( "Network_mgr:srv_register srvid not match,expect %s,got %d",
            g_app.srvid,srvid )
        return
    end

    if self.srv[pkt.session] then
        ELOG( "Network_mgr:srv_register session conflict:%d",pkt.session )
        return false
    end

    self.srv[pkt.session] = conn
    network_mgr:set_conn_session( conn.conn_id,pkt.session )
    return true
end

-- 检查一个连接超时
local pkt = {response = true}
function Network_mgr:check_one_timeout( srv_conn,check_time )
    if not srv_conn.conn_ok then return end -- 还没连接成功的不检查

    local ts = srv_conn:check( check_time )
    if ts > SRV_ALIVE_TIMES then
        PLOG( "%s server timeout",srv_conn:conn_name() )

        self:close_srv_conn( srv_conn )
        if srv_conn.auto_conn then self.srv_waiting[srv_conn] = 1 end
    elseif ts > 0 and srv_conn.auth then
        srv_conn:send_pkt( SS.SYS_BEAT,pkt )
    end
end

-- 定时器回调
function Network_mgr:do_timer()
    -- 重连
    local waiting = self.srv_waiting
    if not table.empty( waiting ) then self.srv_waiting = {} end
    for conn in pairs( waiting ) do
        self:reconnect_srv( conn )
    end

    local check_time = ev:time() - SRV_ALIVE_INTERVAL
    for conn_id,srv_conn in pairs( self.srv_conn ) do
        self:check_one_timeout( srv_conn,check_time )
    end
end

-- 获取服务器连接
function Network_mgr:get_srv_conn( session )
    return self.srv[session]
end

-- 获取网关连接(在非网关服务器进程获取)
function Network_mgr:get_gateway_conn()
    return self.srv[gateway_session]
end

-- 设置客户端连接
function Network_mgr:bind_role( pid,clt_conn )
    assert( "player already have a conn",nil == self.clt[pid] )

    clt_conn:bind_role( pid )
    self.clt[pid] = clt_conn
    network_mgr:set_conn_owner( clt_conn.conn_id,pid )
end

-- 获取客户端连接
function Network_mgr:get_clt_conn( pid )
    return self.clt[pid]
end

-- 根据conn_id获取连接
function Network_mgr:get_conn( conn_id )
    return self.clt_conn[conn_id] or self.srv_conn[conn_id]
end

-- 根据服务器名字发送
function Network_mgr:srv_name_send( name,cmd,pkt,ecode )
    local session = self.name_srv[name]
    if not session then
        return error( "no such server:" .. name )
    end

    local srv_conn = self.srv[session]
    if not srv_conn then
        ELOG( "srv_send no conn found:%d",cmd )
        return
    end

    srv_conn:send_pkt( cmd,pkt,ecode )
end

-- 主动关闭服务器链接
function Network_mgr:close_srv_conn( conn )
    conn:close()
    g_conn_mgr:set_conn( conn.conn_id,nil )

    if conn.session then self.srv[conn.session] = nil end
    self.srv_conn[conn_id] = nil
end

-- 服务器广播
function Network_mgr:srv_multicast( cmd,pkt,ecode )
    local conn_list = {}
    for _,conn in pairs( self.srv ) do
        table.insert( conn_list,conn.conn_id )
    end
    return network_mgr:srv_multicast( 
        conn_list,network_mgr.CDC_PROTOBUF,cmd,ecode or 0,pkt )
end

-- 客户端广播
-- 非网关向客户端广播，由网关转发.网关必须处理clt_multicast_new回调
-- @mask:掩码，参考C++宏定义clt_multicast_t。
-- @args_list:参数列表，根据掩码，这个参数可能是玩家id，也可能是自定义参数
--  如果是玩家id，网关底层会自动转发。如果是自定义参数，如连接id，等级要求...
-- 回调clt_multicast_new时会把args_list传回脚本，需要脚本定义处理方式
function Network_mgr:clt_multicast( mask,args_list,cmd,pkt,ecode )
    local srv_conn = self:get_gateway_conn()
    return network_mgr:ssc_multicast( srv_conn.conn_id,
        mask,args_list,network_mgr.CDC_PROTOBUF,cmd,ecode or 0,pkt )
end

-- 客户端广播(直接发给客户端，仅网关可用)
-- @conn_list: 客户端conn_id列表
function Network_mgr:raw_clt_multicast( conn_list,cmd,pkt,ecode )
    return network_mgr:clt_multicast( 
        conn_list,network_mgr.CDC_PROTOBUF,cmd,ecode or 0,pkt )
end

-- 底层accept回调
function Network_mgr:srv_conn_accept( conn_id,conn )
    self.srv_conn[conn_id] = conn

    PLOG( "accept server connection:%d",conn_id )

    conn:send_register()
end

-- 新增客户端连接
function Network_mgr:clt_conn_accept( conn_id,conn )
    self.clt_conn[conn_id] = conn

    PLOG( "accept client connection:%d",conn_id )
end

-- 服务器之间连接成功
function Network_mgr:srv_conn_new( conn_id,ecode )
    local conn = self.srv_conn[conn_id]
    if 0 ~= ecode then
        PLOG( "server connect(%d) error:%s",conn_id,util.what_error( ecode ) )
        self.srv_conn[conn_id] = nil

        if conn.auto_conn then self.srv_waiting[conn] = 1 end
        return
    end

    PLOG( "server connect (%d) establish",conn_id)

    conn:send_register()
end

-- 底层连接断开回调
function Network_mgr:srv_conn_del( conn_id )
    local conn = self.srv_conn[conn_id]

    if conn.session then self.srv[conn.session] = nil end
    self.srv_conn[conn_id] = nil

    PLOG( "%s connect del",conn:conn_name() )

    -- TODO: 是否有必要检查对方是否主动关闭
    if conn.auto_conn then self.srv_waiting[conn] = 1 end
end

-- 客户端连接断开回调
function Network_mgr:clt_conn_del( conn_id )
    local conn = self.clt_conn[conn_id]

    self.clt_conn[conn_id] = nil

    g_account_mgr:role_offline( conn_id )

    if conn.pid then
        self.clt[conn.pid] = nil
        local pkt = { pid = conn.pid }
        g_network_mgr:srv_multicast( SS.PLAYER_OFFLINE,pkt )
    end

    PLOG( "client connect del:%d",conn_id )
end

-- 此函数必须返回一个value为玩家id的table
-- CLTCAST定义在define.lua
function clt_multicast_new( mask,... )
    if mask == CLTCAST.WORLD then
        local pid_list = {}
        for pid in pairs( g_network_mgr.clt ) do
            table.insert( pid_list,pid )
        end
        return pid_list
    elseif mask == CLTCAST.LEVEL then
    end
end

local _network_mgr = Network_mgr()
return _network_mgr
