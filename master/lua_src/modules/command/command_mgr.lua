-- 消息管理

local ss_map = {}
local sc_map = {}
local cs_map = {}

local ss_list  = require "proto.ss_command"
local cmd_list = require "proto.proto"

-- 协议使用太频繁，放到全局变量，方便调用
SS = {}
for k,v in pairs( ss_list ) do
    -- 使用时只需要一个值就可以了，没必要传一个table
    -- PLAYER.LOGIN是一个值而不是一个table
    SS[k] = v[1]
    ss_map[ v[1] ] = v
end

SC = {}
for k,v in pairs( cmd_list[1] ) do
    SC[k] = v[1]
    sc_map[ v[1] ] = v
end

CS = {}
for k,v in pairs( cmd_list[2] ) do
    CS[k] = v[1]
    cs_map[ v[1] ] = v
end

local SC = SC
local CS = CS
local SS = SS

local network_mgr = network_mgr -- 这个是C++底层的网络管理对象
-- 对于CS、SS数据包，因为要实现现自动转发，在注册回调时设置,因为要记录sesseion
-- SC数据包则需要在各个进程设置到C++，这样就能在所有进程发协议给客户端
for _,v in pairs( cmd_list[1] ) do
    network_mgr:set_sc_cmd( v[1],v[2],v[3],0,0 )
end

local SESSION = g_app.session

local g_rpc   = g_rpc
local g_network_mgr = g_network_mgr

local Command_mgr = oo.singleton( nil,... )

function Command_mgr:__init()
    self.ss = {} -- 记录服务器之间回调函数
    for _,v in pairs( SS ) do
        self.ss[ v ] = {}
    end

    self.cs = {} -- 记录客户端-服务器之间的回调函数
    for _,v in pairs( CS ) do
        self.cs[ v ] = {}
    end

    -- 引用授权数据，防止频繁调用函数
    self.auth_pid = g_authorize:get_player_data()
end

-- 加载二进制flatbuffers schema文件
function Command_mgr:load_schema()
    local pfs = network_mgr:load_one_schema( network_mgr.CDC_PROTOBUF,"pb" )
    PRINTF( "load protocol schema:%d",pfs )

    local ffs = network_mgr:load_one_schema( network_mgr.CDC_FLATBUF,"fbs" )
    PRINTF( "load flatbuffers schema:%d",ffs )

    return (pfs >= 0 and ffs >= 0)
end

-- 注册客户端协议处理
function Command_mgr:clt_register( cmd,handler,noauth )
    local cfg = self.cs[cmd]
    if not cfg then
        return error( "clt_register:cmd not define" )
    end

    cfg.handler = handler
    cfg.noauth  = noauth  -- 处理此协议时，不要求该链接可信

    local raw_cfg = cs_map[cmd]
    network_mgr:set_cs_cmd( raw_cfg[1],raw_cfg[2],raw_cfg[3],0,SESSION )
end

-- 注册服务器协议处理
-- @noauth    -- 处理此协议时，不要求该链接可信
-- @noreg     -- 此协议不需要注册到其他服务器
function Command_mgr:srv_register( cmd,handler,noreg,noauth )
    local cfg = self.ss[cmd]
    if not cfg then
        return error( "srv_register:cmd not define" )
    end

    cfg.handler  = handler
    cfg.noauth   = noauth
    cfg.noreg    = noreg

    local raw_cfg = ss_map[cmd]
    network_mgr:set_ss_cmd( raw_cfg[1],raw_cfg[2],raw_cfg[3],0,SESSION )
end

-- 本进程需要注册的指令
function Command_mgr:command_pkt()
    local pkt = {}
    pkt.clt_cmd = self:clt_cmd()
    pkt.srv_cmd = self:srv_cmd()
    pkt.rpc_cmd = g_rpc:rpc_cmd()

    return pkt
end

-- 发分服务器协议
function Command_mgr:srv_dispatch( srv_conn,cmd,... )
    local cfg = self.ss[cmd]

    local handler = cfg.handler
    if not cfg.handler then
        return ERROR( "srv_dispatch:cmd [%d] no handle function found",cmd )
    end

    if not srv_conn.auth and not cfg.noauth then
        return ERROR( "clt_dispatch:try to call auth cmd [%d]",cmd )
    end

    return handler( srv_conn,... )
end

-- 分发协议
function Command_mgr:clt_dispatch( clt_conn,cmd,... )
    local cfg = self.cs[cmd]

    local handler = cfg.handler
    if not cfg.handler then
        return ERROR( "clt_dispatch:cmd [%d] no handle function found",cmd )
    end

    if not clt_conn.auth and not cfg.noauth then
        return ERROR( "clt_dispatch:try to call auth cmd [%d]",cmd )
    end

    return handler( clt_conn,... )
end

-- 分发网关转发的客户端协议
function Command_mgr:clt_dispatch_ex( srv_conn,pid,cmd,... )
    local cfg = self.cs[cmd]

    local handler = cfg.handler
    if not cfg.handler then
        return ERROR( "clt_dispatch_ex:cmd [%d] no handle function found",cmd )
    end

    -- 判断这个服务器连接是已认证的
    if not srv_conn.auth then
        return ERROR( "clt_dispatch_ex:srv conn not auth,cmd [%d]",cmd )
    end

    -- 判断这个玩家是已认证的
    if not cfg.noauth and not self.auth_pid[pid] then
        return ERROR(
            "clt_dispatch_ex:player not auth,pid [%d],cmd [%d]",pid,cmd )
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

-- 其他服务器指令注册
function Command_mgr:other_cmd_register( srv_conn,pkt )
    local session = srv_conn.session

    -- 记录该服务器所处理的cs指令
    for _,cmd in pairs( pkt.clt_cmd or {} ) do
        local _cfg = self.cs[cmd]
        assert( _cfg,"other_cmd_register no such clt cmd" )
        if not g_app.ok then -- 启动的时候检查一下，热更则覆盖
            assert( _cfg,"other_cmd_register clt cmd register conflict" )
        end

        _cfg.session = session
        local raw_cfg = cs_map[cmd]
        network_mgr:set_cs_cmd( raw_cfg[1],raw_cfg[2],raw_cfg[3],0,session )
    end

    -- 记录该服务器所处理的ss指令
    for _,cmd in pairs( pkt.srv_cmd or {} ) do
        local _cfg = self.ss[cmd]
        assert( _cfg,"other_cmd_register no such srv cmd" )
        assert( _cfg,"other_cmd_register srv cmd register conflict" )

        _cfg.session = session
        local raw_cfg = ss_map[cmd]
        network_mgr:set_ss_cmd( raw_cfg[1],raw_cfg[2],raw_cfg[3],0,session )
    end

    -- 记录该服务器所处理的rpc指令
    for _,cmd in pairs( pkt.rpc_cmd or {} ) do
        g_rpc:register( cmd,session )
    end

    PRINTF( "register cmd from %s",srv_conn:conn_name() )

    return true
end

local command_mgr = Command_mgr()

return command_mgr
