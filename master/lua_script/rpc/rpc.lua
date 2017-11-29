-- rpc client and server

local LIMIT = require "global.limits"
local g_network_mgr = g_network_mgr

local Rpc = oo.singleton( nil,... )

function Rpc:__init()
    self.call = {}
    self.seed = 1
end

-- 声明一个rpc调用
function Rpc:declare( method_name,func )
    if self.call[method_name] then
        return error(
            string.format( "rpc:conflicting declaration:%s",method_name ) )
    end

    self.call[method_name] = {}
    self.call[method_name].func = func
end

-- 其他服务器注册rpc回调
function Rpc:register( method_name,session )
    assert( nil == self.call[method_name],
        string.format( "rpc already exist:%s",method_name ) )
    
    self.call[method_name] = {}
    self.call[method_name].session = session
end

-- 发起rpc调用(无返回值)
-- rpc:invoke( "addExp",pid,exp )
function Rpc:invoke( method_name,... )
    local cfg = self.call[method_name]
    if not cfg then
        return error(
            string.format( "rpc:\"%s\" was not declared",method_name ) )
    end

    local srv_conn = g_network_mgr:get_srv_conn( cfg.session )
    if not srv_conn then
        return error( string.format( 
            "rpc:no connection to remote server:%s,%d",method_name,cfg.session))
    end

    return srv_conn:send_rpc_pkt( 0,method_name,... )
end

-- 发起rpc调用(有返回值)
-- rpc:xinvoke( "addExp",callback,callback_param,pid,exp )
function Rpc:xinvoke( method_name,callback,callback_param,... )
    local cfg = self.call[method_name]
    if not cfg then
        return error(
            string.format( "rpc:\"%s\" was not declared",method_name ) )
    end

    local srv_conn = g_network_mgr:get_srv_conn( cfg.session )
    if not srv_conn then
        return error( string.format( 
            "rpc:no connection to remote server:%s,%d",method_name,cfg.session))
    end

    srv_conn:send_rpc_pkt( self.seed,method_name,... )

    self.seed = self.seed + 1
    if self.seed > LIMIT.INT32_MAX then self.seed = 1 end

    -- TODO: 这里需要记录回调信息
end

-- 获取当前服务器的所有rpc调用
function Rpc:rpc_cmd()
    local cmds = {}

    for method_name,cfg in pairs( self.call ) do
        if cfg.func then table.insert( cmds,method_name ) end
    end

    return cmds
end

local rpc = Rpc()

-- 底层回调，这样可以很方便地处理可变参而不需要创建一个table来处理参数，减少gc压力
function rpc_command_new( conn_id,rpc_id,method_name,... )
    print( "raw_dispatch ",method_name,... )
    local cfg = rpc.call[method_name]
    if not cfg then
        return error( string.format( "rpc:\"%s\" was not declared",method_name ) )
    end

    return cfg.func( ... )
end

function rpc_command_return ( conn_id,rpc_id,ecode,... )
    print( "Rpc:response ====>>>>>>>>>>>>>>>>>",conn_id,rpc_id,ecode,... )
end

return rpc