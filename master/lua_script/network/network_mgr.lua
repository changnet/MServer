-- network_mgr 网络连接管理

local Timer         = require "Timer"
local Stream_socket = require "Stream_socket"
local Srv_conn      = oo.refer( "network/srv_conn" )

-- 服务器名字转索引，不经常改。运维也不需要知道，暂时不做成配置
local name_type =
{
    gateway = 1,
    world   = 2,
    test    = 3,
    example = 4
}

local Network_mgr = oo.singleton( nil,... )

function Network_mgr:__init()
    self.srv = {}  -- 已认证的服务器连接
    self.clt = {}  -- 已认证的客户端连接

    self.srv_conn = {} -- 未认证的服务器连接
    self.clt_conn = {} -- 未认证的客户端连接

    self.srv_listen = nil

    self.timer = Timer()
    self.timer:set_self( self )
    self.timer:set_callback( self.do_timer )

    self.timer:start( 5,5 )
end

-- 生成服务器session id
-- @name  服务器名称，如gateway、world...
-- @index 服务器索引，如可能有多个gateway，分别为1,2...
-- @srvid 服务器id，与运维相关。开了第N个服
function Network_mgr:generate_srv_session( name,index,srvid )
    local ty = name_type[name]

    assert( ty,"server name type not define" )
    assert( index < (1 << 24),"server index out of boundry" )
    assert( srvid < (1 << 16),   "server id out of boundry" )

    -- int32 ,8bits is ty,8bits is index,16bits is srvid
    return (ty << 24) + (index <<16) + srvid
end

--  监听服务器连接
function Network_mgr:srv_listen( ip,port )
    local conn = Stream_socket()
    conn:set_self_ref( self )
    conn:set_on_acception( Network_mgr.on_srv_acception )

    local fd = conn:listen( ip,port )
    if not fd then return false end

    self.srv_listen = conn
    PLOG( "server listen at %s:%d",ip,port )

    return true
end

-- 处理服务器连接
function Network_mgr:on_srv_acception( conn )
    local srv_conn = Srv_conn( conn )
    self.srv_conn[srv_conn] = ev:time()

    local fd = conn:file_description()
    PLOG( "accept server connection,fd:%d",fd )
end

-- 主动连接其他服务器
function Network_mgr:connect_srv( srvs )
    for _,srv in pairs( srvs ) do
        local srv_conn = Srv_conn()
        srv_conn:connect( srv.ip,srv.port )

        self.srv_conn[srv_conn] = ev:time()
        PLOG( "server connect to %s:%d",srv.ip,srv.port )
    end
end

-- 服务器断开
function Network_mgr:srv_disconnect( conn )
    PLOG( "server disconnect" )
    self.srv_conn[conn] = nil
end

-- 服务器认证
function Network_mgr:srv_register( conn,pkt )
    self.srv_conn[conn] = nil

    if pkt.auth ~= "====>>>>MD5<<<<====" then
        ELOG( "Network_mgr:srv_register fail,session %d",pkt.session )
        return false
    end

    if self.srv[pkt.session] then
        ELOG( "Network_mgr:srv_register session conflict:%d",pkt.session )
        return false
    end

    self.srv[pkt.session] = conn
    return true
end

-- 发起认证
function Network_mgr:register_pkt( message_mgr )
    local pkt =
    {
        session = Main.session,
        timestamp = ev:time(),
        auth = "====>>>>MD5<<<<====",
        clt_msg = { 1,2,3 },
        srv_msg = { 4,5,6 },
        rpc_msg = { "abc","def" }
    }

    pkt.clt_cmd = message_mgr:clt_cmd()
    pkt.srv_cmd = message_mgr:srv_cmd()

    return pkt
end

-- 定时器回调
function Network_mgr:do_timer()
    print( "network do timer",ev:time() )
end

local network_mgr = Network_mgr()

return network_mgr
