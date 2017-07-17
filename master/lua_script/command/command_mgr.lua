-- 消息管理

local SC = SC
local CS = CS
local SS = SS

local SESSION = Main.session

local g_rpc   = g_rpc
local g_network_mgr = g_network_mgr

local network_mgr = network_mgr
local Command_mgr = oo.singleton( nil,... )

function Command_mgr:__init()
    self.ss = {}
    for _,v in pairs( SS ) do
        self.ss[ v ] = {}
    end

    self.cs = {}
    for _,v in pairs( CS ) do
        self.cs[ v ] = {}
    end
end

-- 加载二进制flatbuffers schema文件
function Command_mgr:load_schema()
    local fs = network_mgr:load_schema( "pb" )
    PLOG( "%s load protocol schema:%d",Main.srvname,fs )
end

-- 注册客户端协议处理
function Command_mgr:clt_register( cmd,handler,noauth )
    local cfg = self.cs[cmd]
    if not cfg then
        return error( "clt_register:cmd not define" )
    end

    cfg.handler = handler
    cfg.noauth  = noauth  -- 处理此协议时，不要求该链接可信

    local raw_cfg = g_command_pre:get_cs_cmd( cmd )
    network_mgr:set_cs_cmd( raw_cfg[1],raw_cfg[2],raw_cfg[3],0,SESSION )
end

-- 注册服务器协议处理
-- @noauth    -- 处理此协议时，不要求该链接可信
-- @noreg     -- 此协议不需要注册到其他服务器
-- @nounpack  -- 此协议不要自动解包
function Command_mgr:srv_register( cmd,handler,noreg,noauth,nounpack )
    local cfg = self.ss[cmd]
    if not cfg then
        return error( "srv_register:cmd not define" )
    end

    cfg.handler  = handler
    cfg.noauth   = noauth
    cfg.noreg    = noreg
    cfg.nounpack = nounpack

    local raw_cfg = g_command_pre:get_ss_cmd( cmd )
    network_mgr:set_ss_cmd( raw_cfg[1],raw_cfg[2],raw_cfg[3],0,SESSION )
end

-- 本进程需要注册的指令
function Command_mgr:command_pkt()
    local pkt =
    {
        name    = Main.srvname,
        session = Main.session,
        timestamp = ev:time(),
    }
    pkt.auth = util.md5( SRV_KEY,pkt.timestamp,pkt.session )

    pkt.clt_cmd = self:clt_cmd()
    pkt.srv_cmd = self:srv_cmd()
    pkt.rpc_cmd = self:rpc_cmd()

    return pkt
end

-- 发分服务器协议
function Command_mgr:srv_dispatch( srv_conn,cmd,... )
    local cfg = self.ss[cmd]

    local handler = cfg.handler
    if not cfg.handler then
        return ELOG( "srv_dispatch:cmd [%d] no handle function found",cmd )
    end

    if not srv_conn.auth and not cfg.noauth then
        return ELOG( "clt_dispatch:try to call auth cmd [%d]",cmd )
    end

    return handler( srv_conn,... )
end

-- 分发协议
function Command_mgr:clt_dispatch( clt_conn,cmd,... )
    local cfg = self.cs[cmd]

    local handler = cfg.handler
    if not cfg.handler then
        return ELOG( "clt_dispatch:cmd [%d] no handle function found",cmd )
    end

    if not clt_conn.auth and not cfg.noauth then
        return ELOG( "clt_dispatch:try to call auth cmd [%d]",cmd )
    end

    return handler( clt_conn,... )
end

-- 分发网关转发的客户端协议
function Command_mgr:clt_dispatch_ex( srv_conn,pid,cmd,... )
    local cfg = self.cs[cmd]

    local handler = cfg.handler
    if not cfg.handler then
        return ELOG( "clt_dispatch_ex:cmd [%d] no handle function found",cmd )
    end

    if not srv_conn.auth then
        return ELOG( "clt_dispatch_ex:srv conn not auth,cmd [%d]",cmd )
    end

    return handler( srv_conn,pid,... )
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
    return g_rpc:rpc_cmd()
end

-- 服务器注册
function Command_mgr:command_register( srv_conn,pkt )
    local session = pkt.session

    -- 记录该服务器所处理的cs指令
    for _,cmd in pairs( pkt.clt_cmd or {} ) do
        local _cfg = self.cs[cmd]
        assert( _cfg,"do_srv_register no such clt cmd" )
        assert( _cfg,"do_srv_register clt cmd register conflict" )

        _cfg.session = session
        local raw_cfg = g_command_pre:get_cs_cmd( cmd )
        network_mgr:set_cs_cmd( raw_cfg[1],raw_cfg[2],raw_cfg[3],0,session )
    end

    -- 记录该服务器所处理的ss指令
    for _,cmd in pairs( pkt.srv_cmd or {} ) do
        local _cfg = self.ss[cmd]
        assert( _cfg,"do_srv_register no such srv cmd" )
        assert( _cfg,"do_srv_register srv cmd register conflict" )

        _cfg.session = session
        local raw_cfg = g_command_pre:get_ss_cmd( cmd )
        network_mgr:set_ss_cmd( raw_cfg[1],raw_cfg[2],raw_cfg[3],0,session )
    end

    -- 记录该服务器所处理的rpc指令
    for _,cmd in pairs( pkt.rpc_cmd or {} ) do
        g_rpc:register( cmd,session )
    end

    return true
end

-- 服务器广播
-- TODO 底层要做个服务器广播
function Command_mgr:srv_broadcast( cfg,pkt )
    for _,srv_conn in pairs( g_network_mgr.srv ) do
        srv_conn:send_pkt( cfg,pkt )
    end
end

local command_mgr = Command_mgr()

return command_mgr
