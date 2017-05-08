-- rpc client and server

local g_network_mgr = g_network_mgr

local Rpc = oo.singleton( nil,... )

function Rpc:__init()
    self.call = {}
end

-- 声明一个rpc调用
function Rpc:declare( name,func )
    if self.call[name] then
        return error( string.format( "rpc:conflicting declaration:%s",name ) )
    end

    self.call[name] = {}
    self.call[name].func = func
end

-- 其他服务器注册rpc回调
function Rpc:register( name,session )
    assert( nil == self.call[name],
        string.format( "rpc already exist:%s",name ) )
    
    self.call[name] = {}
    self.call[name].session = session
end

-- 发起rpc调用(无返回值)
-- rpc:invoke( "addExp",pid,exp )
function Rpc:invoke( name,... )
    local cfg = self.call[name]
    if not cfg then
        return error( string.format( "rpc:\"%s\" was not declared",name ) )
    end

    local srv_conn = g_network_mgr:get_srv_conn( cfg.session )
    if not srv_conn then
        return error( string.format( 
            "rpc:no connection to remote server:%s,%d",name,cfg.session ) )
    end

    return srv_conn:send_rpc( 0,name,... )
end

-- 发起rpc调用(有返回值)
-- rpc:xinvoke( "addExp",callback,callback_param,pid,exp )
function Rpc:xinvoke( name,callback,callback_param,... )
    local cfg = self.call[name]
    if not cfg then
        return error( string.format( "rpc:\"%s\" was not declared",name ) )
    end

    local srv_conn = g_network_mgr:get_srv_conn( cfg.session )
    if not srv_conn then
        return error( string.format( 
            "rpc:no connection to remote server:%s,%d",name,cfg.session ) )
    end

    return srv_conn:send_rpc( 0,name,... )
end

-- 获取当前服务器的所有rpc调用
function Rpc:rpc_cmd()
    local cmds = {}

    for name,cfg in pairs( self.call ) do
        if cfg.func then table.insert( cmds,name ) end
    end

    return cmds
end

local rpc = Rpc()

-- 底层回调，这样可以很方便地处理可变参而不需要创建一个table来处理参数，减少gc压力
function rpc_command_new( conn_id,rpc_id,name,... )
    print( "raw_dispatch ",name,... )
    local cfg = rpc.call[name]
    if not cfg then
        return error( string.format( "rpc:\"%s\" was not declared",name ) )
    end

    return cfg.func( ... )
end

function rpc_command_return ( conn_id,rpc_id,ecode,... )
    PLOG( "Rpc:response ====>>>>>>>>>>>>>>>>>" )
end

return rpc