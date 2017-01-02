-- rpc client and server

local rpc_ivk = SS.RPC_IVK[1]
local rpc_dph = SS.RPC_DPH[1]

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

-- client发起rpc调用
-- 回调 ？？也就是否有返回值
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

    return srv_conn.conn:rpc_send( rpc_ivk,id,... )
end

-- 处理rpc请求
function Rpc:dispatch( srv_conn )
    local pkt = srv_conn.conn:rpc_decode( rpc_dph )
    local cfg = self.call[pkt.name]
    if not cfg then
        return error( string.format( "rpc:\"%s\" was not declared",name ) )
    end

    cfg.func( table.unpack(pkt.param) )
end

local rpc = Rpc()

return rpc