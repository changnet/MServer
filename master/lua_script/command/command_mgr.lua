-- 消息管理

local lua_flatbuffers = require "lua_flatbuffers"

-- 协议使用太频繁，放到全局变量
local sc = require "command/sc_command"
SC,CS = sc[1],sc[2]

SS    = require "command/ss_command"

local SC = SC
local CS = CS
local SS = SS

local CLT_CMD = SS.CLT_CMD[1]
local RPC_REQ = SS.RPC_REQ[1]
local RPC_RES = SS.RPC_RES[1]

local SESSION = Main.session

local rpc = require "rpc/rpc"
local network_mgr = require "network/network_mgr"

local Command_mgr = oo.singleton( nil,... )

function Command_mgr:__init()
    self.ss = {}
    for _,v in pairs( SS ) do
        self.ss[ v[1] ] = v
    end

    self.cs = {}
    for _,v in pairs( CS or {} ) do
        self.cs[ v[1] ] = v
    end

    self.lfb = lua_flatbuffers()
end

-- 加载二进制flatbuffers schema文件
function Command_mgr:load_schema()
    return self.lfb:load_bfbs_path( "fbs","bfbs" )
end

-- 初始化协议定义
function Command_mgr:init_command()
    -- 这个不能放在文件头部或者__init函数中require
    -- 因为其他文件一般都需要引用command_mgr本身，造成循环require
    require "command/command_header"
end

-- 注册客户端协议处理
function Command_mgr:clt_register( cfg,handler,noauth )
    if not self.cs[cfg[1]] then
        return error( "clt_register:cmd not define" )
    end

    cfg.handler = handler
    cfg.noauth  = noauth  -- 处理此协议时，不要求该链接可信
end

-- 注册服务器协议处理
-- @noauth    -- 处理此协议时，不要求该链接可信
-- @noreg     -- 此协议不需要注册到其他服务器
-- @nounpack  -- 此协议不要自动解包
function Command_mgr:srv_register( cfg,handler,noreg,noauth,nounpack )
    if not self.ss[cfg[1]] then
        return error( "srv_register:cmd not define" )
    end

    cfg.handler  = handler
    cfg.noauth   = noauth
    cfg.noreg    = noreg
    cfg.nounpack = nounpack
end

-- 分发服务器协议
function Command_mgr:srv_dispatcher( cmd,pid,srv_conn )
    local cfg = self.ss[cmd]
    if not cfg then
        return ELOG( "srv_dispatcher:cmd [%d] not define",cmd )
    end

    local handler = cfg.handler
    if not handler then
        return ELOG( 
            "srv_dispatcher:cmd [%d] define but no handler register",cmd )
    end

    -- 不需要解包的协议，可能是rpc等特殊的数据包
    if cfg.nounpack then
        return handler( srv_conn,pid )
    end

    local pkt = 
        srv_conn.conn:ss_flatbuffers_decode( self.lfb,cmd,cfg[2],cfg[3] )

    return handler( srv_conn,pkt )
end

-- 分发服务器协议
function Command_mgr:srv_unauthorized_dispatcher( cmd,pid,srv_conn )
    -- server to server command handle here
    local cfg = self.ss[cmd]
    if not cfg then
        return ELOG( "srv_unauthorized_dispatcher:cmd [%d] not define",cmd )
    end

    if not cfg.noauth then
        assert( false,"no auth" )
        return ELOG( 
            "srv_unauthorized_dispatcher:try to call auth cmd [%d]",cmd )
    end

    local handler = cfg.handler
    if not handler then
        return ELOG( 
            "srv_unauthorized:cmd [%d] define but no handler register",cmd )
    end
    local pkt = 
        srv_conn.conn:ss_flatbuffers_decode( self.lfb,cmd,cfg[2],cfg[3] )
    return handler( srv_conn,pkt )
end

-- 处理来自gateway转发的客户端包
function Command_mgr.css_dispatcher( self )
    return function( srv_conn,pid )
        local cmd = srv_conn.conn:css_cmd()
        if not cmd then
            return ELOG( "Command_mgr:css_dispatcher no cmd found" )
        end

        local cfg = self.cs[cmd]
        if not cfg then
            return ELOG( "css_dispatcher:command [%d] not define",cmd )
        end

        local handler = cfg.handler
        if not handler then
            return ELOG( 
                "css_dispatcher:cmd [%d] define but no handler register",cmd )
        end

        local pkt = 
            srv_conn.conn:css_flatbuffers_decode( self.lfb,cmd,cfg[2],cfg[3] )
        return handler( srv_conn,pkt )
    end
end

-- 客户端未认证连接指令
-- TODO:指令来自未认证的连接，暂不考虑转发
function Command_mgr:clt_unauthorized_cmd( cmd,clt_conn )
    -- client to server command handle here
    local cfg = self.cs[cmd]
    if not cfg then
        return ELOG( "clt_unauthorized_cmd:cmd [%d] not define",cmd )
    end

    if not cfg.noauth then
        return ELOG( "clt_unauthorized_cmd:try to call auth cmd [%d]",cmd )
    end

    local handler = cfg.handler
    if handler then
        --  如果存在handle，说明是在当前进程处理该协议
        local pkt = 
            clt_conn.conn:cs_flatbuffers_decode( self.lfb,cmd,cfg[2],cfg[3] )
        return handler( clt_conn,pkt )
    end

    ELOG( "clt_unauthorized_cmd:no handler found [%d]",cmd )
end

-- 分发客户端指令
function Command_mgr:clt_invoke( cmd,clt_conn )
    -- client to server command handle here
    local cfg = self.cs[cmd]
    if not cfg then
        return ELOG( "clt_invoke:cmd [%d] not define",cmd )
    end

    local handler = cfg.handler
    if handler then
        --  如果存在handle，说明是在当前进程处理该协议
        local pkt = 
            clt_conn.conn:cs_flatbuffers_decode( self.lfb,cmd,cfg[2],cfg[3] )
        return handler( clt_conn,pkt )
    end

    -- 转发到其他服务器
    local srv_conn = network_mgr:get_srv_conn( cfg.session )
    if not srv_conn then
        return ELOG( "clt_invoke:no handler found [%d]",cmd )
    end

    return srv_conn.conn:css_flatbuffers_send( 
            clt_conn.pid,CLT_CMD,clt_conn.conn )
end

-- 转发其他服务器数据包客到户端
function Command_mgr.ssc_tansport( srv_conn,pid )
    local clt_conn = network_mgr:get_clt_conn( pid )
    if not clt_conn then
        return ELOG( "Command_mgr:ssc_tansport no clt conn found" )
    end

    srv_conn.conn:css_flatbuffers_send( srv_cmd,clt_conn )
end

-- 发送数据包到gateway，再由它转发给客户端
function Command_mgr:ssc_send( srv_conn,cfg,pid,pkt )
    assert( cfg,"ssc_send no cmd specified" )

    srv_conn.conn:ssc_flatbuffers_send( 
        self.lfb,pid,CLT_CMD,cfg[1],cfg[2],cfg[3],pkt )
end

-- 发送服务器消息
function Command_mgr:srv_send( srv_conn,cfg,pkt )
    assert( cfg,"srv_send no cmd specified" )

    srv_conn.conn:ss_flatbuffers_send( 
        self.lfb,SESSION,cfg[1],cfg[2],cfg[3],pkt )
end

-- 发送客户端消息
function Command_mgr:clt_send( clt_conn,cfg,pkt,errno )
    return clt_conn.conn:sc_flatbuffers_send( 
        self.lfb,cfg[1],cfg[2],cfg[3],errno or 0,pkt )
end

-- 获取当前进程处理的客户端指令
function Command_mgr:clt_cmd()
    local cmds = {}
    for cmd,cfg in pairs( self.cs ) do
        if cfg.handler then table.insert( cmds,cmd ) end
    end

    return cmds
end

-- 获取当前进程处理的服务端指令
function Command_mgr:srv_cmd()
    local cmds = {}
    for cmd,cfg in pairs( self.ss ) do
        if cfg.handler and not cfg.noreg then table.insert( cmds,cmd ) end
    end

    return cmds
end

-- 获取当前进程处理的rpc指令
function Command_mgr:rpc_cmd()
    return rpc:rpc_cmd()
end

-- 服务器注册
function Command_mgr:do_srv_register( srv_conn,pkt )
    local session = pkt.session

    -- 记录该服务器所处理的cs指令
    for _,cmd in pairs( pkt.clt_cmd or {} ) do
        local _cfg = self.cs[cmd]
        assert( _cfg,"do_srv_register no such clt cmd" )
        assert( _cfg,"do_srv_register clt cmd register conflict" )

        _cfg.session = session
    end

    -- 记录该服务器所处理的ss指令
    for _,cmd in pairs( pkt.srv_cmd or {} ) do
        local _cfg = self.ss[cmd]
        assert( _cfg,"do_srv_register no such srv cmd" )
        assert( _cfg,"do_srv_register srv cmd register conflict" )

        _cfg.session = session
    end

    -- 记录该服务器所处理的rpc指令
    for _,cmd in pairs( pkt.rpc_cmd or {} ) do
        rpc:register( cmd,session )
    end

    return true
end

local command_mgr = Command_mgr()

return command_mgr
