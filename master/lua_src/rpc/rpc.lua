-- rpc client and server

local Auto_id = require "modules.system.auto_id"

local Rpc = oo.singleton( nil,... )

function Rpc:__init()
    self.callback = {}
    self.procedure = {}
    self.auto_id = Auto_id()
end

-- 声明一个rpc调用
-- @session：缺省下默认为当前服务器，不能冲突。-1则为不广播，调用时需要指定服务器
function Rpc:declare( method_name,method,session )
    -- 在启动(g_app.ok为false)时检测冲突，热更时覆盖
    if not g_app.ok and self.procedure[method_name] then
        return error(
            string.format( "rpc:conflicting declaration:%s",method_name ) )
    end

    self.procedure[method_name] = {}
    self.procedure[method_name].method = method
    self.procedure[method_name].session = session
end

-- 其他服务器注册rpc回调
function Rpc:register( method_name,session )
    if not g_app.ok then -- 启动的时候检查一下，热更则覆盖
        assert( nil == self.procedure[method_name],
            string.format( "rpc already exist:%s",method_name ) )
    end

    self.procedure[method_name] = {}
    self.procedure[method_name].session = session
end

-- 获取method所在的连接
-- 查找不到则返回nil
function Rpc:get_method_conn( method_name )
    local cfg = self.procedure[method_name]

    if not cfg then return nil end

    return g_network_mgr:get_srv_conn( cfg.session )
end

-- 发起rpc调用(无返回值)
-- rpc:invoke( "addExp",pid,exp )
function Rpc:invoke( method_name,... )
    local srv_conn = self:get_method_conn( method_name )
    if not srv_conn then
        ELOG( "rpc:no connection to remote server:%s",method_name )
        return
    end

    return srv_conn:send_rpc_pkt( 0,method_name,... )
end

-- 发起rpc调用(无返回值)
-- rpc:call( "addExp",pid,exp )
function Rpc:call( srv_conn,method_name,... )
    return srv_conn:send_rpc_pkt( 0,method_name,... )
end

-- 发起rpc调用(有返回值)
-- rpc:xinvoke( "addExp",callback,pid,exp )
function Rpc:xinvoke( method_name,callback,... )
    local srv_conn = self:get_method_conn( method_name )
    if not srv_conn then
        return error( string.format( 
            "rpc:no connection to remote server:%s",method_name))
    end

    return self:xcall( srv_conn,method_name,callback,... )
end

-- 发起rpc调用
-- rpc:xcall( "addExp",callback,pid,exp )
function Rpc:xcall( srv_conn,method_name,callback,... )
    -- 记录回调信息
    local call_id = self.auto_id:next_id( self.callback )

    self.callback[call_id] = callback
    srv_conn:send_rpc_pkt( call_id,method_name,... )
end

-- 获取当前服务器的所有rpc调用
function Rpc:rpc_cmd()
    local cmds = {}

    for method_name,cfg in pairs( self.procedure ) do
        if -1 ~= cfg.session and cfg.method then
            table.insert( cmds,method_name )
        end
    end

    return cmds
end

local rpc = Rpc()

-- 底层回调，这样可以很方便地处理可变参而不需要创建一个table来处理参数，减少gc压力
function rpc_command_new( conn_id,rpc_id,method_name,... )
    local cfg = rpc.procedure[method_name]
    if not cfg then
        return error( string.format( "rpc:\"%s\" was not declared",method_name ) )
    end

    return cfg.method( ... )
end

function rpc_command_return ( conn_id,rpc_id,ecode,... )
    local callback = rpc.callback[rpc_id]
    if not callback then
        ELOG("rpc return no callback found:id = %d",rpc_id)
        return
    end

    return callback( ecode,... )
end

return rpc
