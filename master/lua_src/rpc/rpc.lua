-- rpc client and server

local Auto_id = require "modules.system.auto_id"

local Rpc = oo.singleton( ... )

function Rpc:__init()
    self.callback = {}
    self.procedure = {}
    self.auto_id = Auto_id()

    self.stat = {}
    self.stat_tm = ev:time()
    self.rpc_perf = g_setting.rpc_perf

    self:check_timer()
end

-- 检测是否需要开启定时器定时写统计信息到文件
function Rpc:check_timer()
    if not self.rpc_perf and self.timer then
        g_timer_mgr:del_timer( self.timer )

        self.timer = nil
        return 
    end

    if self.rpc_perf and not self.timer then
        self.timer = g_timer_mgr:new_timer( 1800,1800,self,self.do_timer )
    end

end

function Rpc:do_timer()
    self:serialize_statistic( true )
end

-- 设置统计log文件
function Rpc:set_statistic( perf,reset )
    -- 如果之前正在统计，先写入旧的
    if self.rpc_perf and ( not perf or reset ) then self:serialize_statistic() end

    -- 如果之前没在统计，或者强制重置，则需要重设stat_tm
    if not self.rpc_perf or reset then
        self.stat = {}
        self.stat_tm = ev.time()
    end

    self.rpc_perf = perf

    self:check_timer()
end

-- 更新耗时统计
function Rpc:update_statistic( method_name,ms )
    local stat = self.stat[method_name]
    if not stat then
        stat = { ms = 0, ts = 0, max = 0, min = 0}
        self.stat[method_name] = stat
    end

    stat.ms = stat.ms + ms
    stat.ts = stat.ts + 1
    if ms > stat.max then stat.max = ms end
    if 0 == stat.min or ms < stat.min then stat.min = ms end
end

-- 写入耗时统计到文件
function Rpc:serialize_statistic( reset )
    if not self.rpc_perf then return false end

    local path = string.format( "%s_%s",self.rpc_perf,g_app.srvname )

    local stat_name = {}
    for k in pairs( self.stat ) do table.insert( stat_name,k ) end

    -- 按名字排序，方便对比查找
    table.sort( stat_name )

    g_log_mgr:raw_file_printf( path,
        "%s ~ %s:",time.date(self.stat_tm),time.date(ev:time()))

    -- 方法名 调用次数 总耗时(毫秒) 最大耗时 最小耗时 平均耗时
    g_log_mgr:raw_file_printf( path,
        "%-32s %-16s %-16s %-16s %-16s %-16s",
        "method","count","msec","max","min","avg" )

    for _,name in pairs( stat_name ) do
        local stat = self.stat[name]
        g_log_mgr:raw_file_printf( path,
            "%-32s %-16d %-16d %-16d %-16d %-16d",
            name,stat.ts,stat.ms,stat.max,stat.min,math.ceil(stat.ms/stat.ts))
    end

    if reset then
        self.stat = {}
        self.stat_tm = ev:time()
    end

    return true
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
        ERROR( "rpc:no connection to remote server:%s",method_name )
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
        return error( string.format( "rpc:[%s] was not declared",method_name ) )
    end

    if rpc.rpc_perf then
        local beg = ev:real_ms_time()
        cfg.method( ... )
        return rpc:update_statistic(method_name,ev:real_ms_time() - beg)
    else
        return cfg.method( ... )
    end
end

function rpc_command_return ( conn_id,rpc_id,ecode,... )
    local callback = rpc.callback[rpc_id]
    if not callback then
        ERROR("rpc return no callback found:id = %d",rpc_id)
        return
    end

    return callback( ecode,... )
end

return rpc
