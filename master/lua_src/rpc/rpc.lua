-- rpc调用

--[[
1. 所有rpc调用在起服时都必须向其他进程注册
2. 收到其他进程的rpc注册，会在rpc本身生成一个函数，不是在metatable，而是在数据层self，这样不
   影响热更
3. 无返回调用的时候，可以用 g_rpc:method_name直接调用
4. 需要指定进程或者返回的时候，用 g_rpc:proxy(srv_conn,cb,...):method_name调用
5. proxy这里会引用一些对象，如果调用出错无返回，将导致这些对象无法释放。一般脚本报错，
   底层是有rpc返回的。如果是因为连接断开，可在连接断开时检测处理。因为涉及到数据准确性，后续再
   看看有没有其他方法处理。目前暂不处理
6. proxy里引用的回调函数，可能是需要热更的。这里会把成员函数转换成名字，不影响热更。如果是
   local函数，是没法热更的。建议放在对象中。如果一定要热更，参考name.lua的处理
]]

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
    if self.rpc_perf and ( not perf or reset ) then
        self:serialize_statistic()
    end

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

    g_log_mgr:raw_file_printf( path,"%s","\n\n" )

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

    -- 为了简化调用，会把rpc直接放到self里
    -- 调用方式为:g_rpc:method_name()，因此名字不能和rpc里的名字一样
    if not self.procedure[method_name] and self[method_name] then
        return error(string.format(
            "rpc:conflicting withself declaration:%s",method_name ))
    end

    self.procedure[method_name] = {}
    self.procedure[method_name].method = method
    self.procedure[method_name].session = session
end

-- 获取当前rpc回调、返回的连接
function Rpc:last_conn()
    return g_network_mgr:get_conn( self.last_conn_id )
end

-- 生成一个可变参回调函数
function Rpc:func_chunk(cb_func,...)
    -- 由于回调参数是可变的，rpc返回的参数也是可变的，但显然没有 cb_func( ...,... )
    -- 这种可以把两个可变参拼起来的写法
    -- 全部用table.pack table.unpack会优雅一起，但我认为每次都创建一个table消耗有点大

    local args_count = select( "#",... )

    if 0 == args_count then
        return function( ... )
            return cb_func( ... )
        end
    end

    if 1 == args_count then
        local args1 = ...
        return function( ... )
            return cb_func( args1,... )
        end
    end

    if 2 == args_count then
        local args1,args2 = ...
        return function( ... )
            return cb_func( args1,args2,... )
        end
    end

    if 3 == args_count then
        local args1,args2,args3 = ...
        return function( ... )
            return cb_func( args1,args2,args3,... )
        end
    end

    local args = table.pack( ... )
    return function( ... )
        return cb_func( table.unpack(args),... )
    end
end

-- rpc调用代理，这个是参照了python的设置
-- 通过代理来设置调用的进程、回调函数、回调参数
-- @srv_conn:目标进程的服务器连接，如果rpc是某个进程专有，这个参数可以不传
-- @cb_func:回调函数，如果不需要回调，这个参数可以不传。如果回调函数是一个成员函数，把对象
--          放在后面的参数即可，如：g_rpc:proxy(player.add_exp,player)
function Rpc:proxy(srv_conn,cb_func,...)
    -- proxy设置的参数，用完即失效，如果不失效，应该是哪里逻辑出现了错误
    assert( nil == self.next_conn)
    assert( nil == self.next_cb_func)

    if type(srv_conn) == "function" then
        self.next_cb_func = func_chunk(cb_func,...)
        return self
    end

    self.next_conn = srv_conn
    if cb_func then
        assert( type(cb_func) == "function" )
        self.next_cb_func = func_chunk(cb_func,...)
    end

    return self
end

-- 生成一个可直接调用的rpc函数
function Rpc:invoke_factory(method_name,session)
    return function ( ... )
        local srv_conn = nil
        if -1 == session then
            srv_conn = self.next_conn
            self.next_conn = nil
        else
            assert( nil == self.next_conn )
            srv_conn = g_network_mgr:get_srv_conn( session )
        end

        assert( srv_conn )

        local call_id = 0
        if self.next_cb_func then
            call_id = self.auto_id:next_id( self.callback )
            self.callback[call_id] = self.next_cb_func

            self.next_cb_func = nil
        end

        return srv_conn:send_rpc_pkt( call_id,method_name,... )
    end
end

-- 其他服务器注册rpc回调
function Rpc:register( procedure,session )
    local method_name = procedure.name
    if not g_app.ok then -- 启动的时候检查一下，热更则覆盖
        assert( nil == self.procedure[method_name],
            string.format( "rpc already exist:%s",method_name ) )
    end

    -- procedure里的session一般是-1，表示这个procedure在多个进程同时存在，调用时需要
    -- 指定进程
    local method_session = procedure.session or session

    self.procedure[method_name] = {}
    self.procedure[method_name].session = method_session

    -- 放到self，方便直接调用
    self[method_name] = invoke_factory(method_name,session)
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
        if cfg.method then -- 有method的才是本进程声明的
            table.insert( cmds,{ method = method_name,session = cfg.session } )
        end
    end

    return cmds
end

local rpc = Rpc()

-- 为了实现可变参数不用table.pack，只能再wrap一层
local function args_wrap(method_name,beg,...)
    rpc:update_statistic(method_name,ev:real_ms_time() - beg)
    return ...
end

-- 底层回调，这样可以很方便地处理可变参而不需要创建一个table来处理参数，减少gc压力
function rpc_command_new( conn_id,rpc_id,method_name,... )
    rpc.last_conn_id = conn_id
    local cfg = rpc.procedure[method_name]
    if not cfg then
        return error( string.format( "rpc:[%s] was not declared",method_name ) )
    end

    if rpc.rpc_perf then
        local beg = ev:real_ms_time()
        return args_wrap( method_name,beg,cfg.method( ... ) )
    else
        return cfg.method( ... )
    end
end

function rpc_command_return ( conn_id,rpc_id,ecode,... )
    rpc.last_conn_id = conn_id
    local callback = rpc.callback[rpc_id]
    if not callback then
        ERROR("rpc return no callback found:id = %d",rpc_id)
        return
    end

    return callback( ecode,... )
end

return rpc
