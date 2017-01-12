-- rpc client and server

local rpc_req = SS.RPC_REQ[1]
local rpc_res = SS.RPC_RES[1]

local network_mgr = require "network/network_mgr"

local Rpc = oo.singleton( nil,... )

function Rpc:__init()
    self.call = {}
end

function Rpc:declare( name,func )
    if self.call[name] then
        return error( string.format( "rpc:conflicting declaration:%s",name ) )
    end

    self.call[name] = {}
    self.call[name].func = func
end

-- client发起rpc调用(无返回值)
-- rpc:invoke( "addExp",pid,exp )
function Rpc:invoke( name,... )
    local cfg = self.call[name]
    if not cfg then
        return error( string.format( "rpc:\"%s\" was not declared",name ) )
    end

    local srv_conn = network_mgr:get_srv_conn( cfg.session )
    if not srv_conn then
        return error( string.format( 
            "rpc:no connection to remote server:%s,%d",name,cfg.session ) )
    end

    return srv_conn.conn:rpc_send( rpc_req,0,... )
end

-- client发起rpc调用(有返回值)
-- rpc:xinvoke( "addExp",callback,callback_param,pid,exp )
function Rpc:xinvoke( name,callback,callback_param,... )
    local cfg = self.call[name]
    if not cfg then
        return error( string.format( "rpc:\"%s\" was not declared",name ) )
    end

    local srv_conn = network_mgr:get_srv_conn( cfg.session )
    if not srv_conn then
        return error( string.format( 
            "rpc:no connection to remote server:%s,%d",name,cfg.session ) )
    end

    return srv_conn.conn:rpc_send( rpc_req,0,... )
end

-- 处理rpc请求
function Rpc:dispatch( srv_conn )
    local pkt = srv_conn.conn:rpc_decode( rpc_res )
    local cfg = self.call[pkt.name]
    if not cfg then
        return error( string.format( "rpc:\"%s\" was not declared",name ) )
    end

    cfg.func( table.unpack(pkt.param) )
end

local rpc = Rpc()

return rpc