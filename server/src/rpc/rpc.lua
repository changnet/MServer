-- rpc调用
--[[
1. 旧版参考其他rpc库(如python xmlrpc: https://docs.python.org/3/library/xmlrpc.html)
   做过一个需要注册的rpc，调用时能直接通过函数名调用，并且自动识别目标链接
   如 rpc:foo() 中的foo是注册时生成的函数名，并且自动发往注册的链接

   但这有几个问题
   1. 注册过程繁琐，包括热更、多进程同名(如多个场景进程)
   2. 代码提示、跳转不正常

2. 关于热更
    回调函数直接存的指针，因此是不能热更的，但是单次rpc调用时间很短，也不需要考虑热更
]]

local AutoId = require "modules.system.auto_id"

local Rpc = oo.singleton(...)

function Rpc:__init()
    self.callback = {}

    self.auto_id = AutoId()

    self.stat = {}
    self.stat_tm = ev:time()
    self.rpc_perf = g_setting.rpc_perf
end

function Rpc:do_timer()
    self:serialize_statistic(true)
end

-- 设置统计log文件
function Rpc:set_statistic(perf, reset)
    -- 如果之前正在统计，先写入旧的
    if self.rpc_perf and (not perf or reset) then self:serialize_statistic() end

    -- 如果之前没在统计，或者强制重置，则需要重设stat_tm
    if not self.rpc_perf or reset then
        self.stat = {}
        self.stat_tm = ev.time()
    end

    self.rpc_perf = perf
end

-- 更新耗时统计
function Rpc:update_statistic(method_name, ms)
    local stat = self.stat[method_name]
    if not stat then
        stat = {ms = 0, ts = 0, max = 0, min = 0}
        self.stat[method_name] = stat
    end

    stat.ms = stat.ms + ms
    stat.ts = stat.ts + 1
    if ms > stat.max then stat.max = ms end
    if 0 == stat.min or ms < stat.min then stat.min = ms end
end

-- 写入耗时统计到文件
function Rpc:serialize_statistic(reset)
    if not self.rpc_perf then return false end

    local path = string.format("%s_%s", self.rpc_perf, g_app.name)

    local stat_name = {}
    for k in pairs(self.stat) do table.insert(stat_name, k) end

    -- 按名字排序，方便对比查找
    table.sort(stat_name)

    g_log_mgr:raw_file_printf(path, "%s ~ %s:", time.date(self.stat_tm),
                              time.date(ev:time()))

    -- 方法名 调用次数 总耗时(毫秒) 最大耗时 最小耗时 平均耗时
    g_log_mgr:raw_file_printf(path, "%-32s %-16s %-16s %-16s %-16s %-16s",
                              "method", "count", "msec", "max", "min", "avg")

    for _, name in pairs(stat_name) do
        local stat = self.stat[name]
        g_log_mgr:raw_file_printf(path, "%-32s %-16d %-16d %-16d %-16d %-16d",
                                  name, stat.ts, stat.ms, stat.max, stat.min,
                                  math.ceil(stat.ms / stat.ts))
    end

    g_log_mgr:raw_file_printf(path, "%s.%d end %s", g_app.name, g_app.index,
                              "\n\n")

    if reset then
        self.stat = {}
        self.stat_tm = ev:time()
    end

    return true
end

-- 获取上一次rpc回调、返回的连接
function Rpc:last_conn()
    return g_network_mgr:get_conn(self.last_conn_id)
end

-- 生成一个可变参回调函数
local function func_chunk(cb, ...)
    -- 由于回调参数是可变的，rpc返回的参数也是可变的，但显然没有 cb_func( ...,... )
    -- 这种可以把两个可变参拼起来的写法
    -- 全部用table.pack table.unpack会优雅一起，但我认为每次都创建一个table消耗有点大
    -- 所以现在把常用的几个函数列出来

    local args_count = select("#", ...)

    if 0 == args_count then
        return function(...)
            return cb(...)
        end
    end

    if 1 == args_count then
        local args1 = ...
        return function(...)
            return cb(args1, ...)
        end
    end

    if 2 == args_count then
        local args1, args2 = ...
        return function(...)
            return cb(args1, args2, ...)
        end
    end

    if 3 == args_count then
        local args1, args2, args3 = ...
        return function(...)
            return cb(args1, args2, args3, ...)
        end
    end

    local args = table.pack(...)
    return function(...)
        return cb(table.unpack(args), ...)
    end
end

-- rpc调用代理，通过代理来设置调用的进程、回调函数、回调参数
-- @param cb 回调函数
-- @param ... 回调参数，不需要可以不传
function Rpc:proxy(cb, ...)
    assert(cb)

    self.next_cb_func = func_chunk(cb, ...)
    return self
end

-- 通过链接对象执行rpc调用
-- @param conn 进程之间的socket连接对象
-- @param name 需要调用的函数名
-- @param ... 参数
function Rpc:conn_call(conn, name, ...)
    -- 如果不需要回调，则call_id为0
    local call_id = 0
    if self.next_cb_func then
        call_id = self.auto_id:next_id(self.callback)
        self.callback[call_id] = self.next_cb_func

        self.next_cb_func = nil
    end

    return conn:send_rpc_pkt(call_id, method_name, ...)
end

-- 通过session执行rpc调用
-- @param session 进程的session，如GATEWAY
-- @param name 需要调用的函数名
-- @param ... 参数
function Rpc:call(session, name, ...)
    local conn = g_network_mgr:get_srv_conn(session)

    return self:conn_call(conn, name, ...)
end

-- /////////////////////////////////////////////////////////////////////////////
-- RPC基础功能在上面处理
-- /////////////////////////////////////////////////////////////////////////////

local rpc = Rpc()

local name_to_func = name_to_func

-- 为了实现可变参数不用table.pack，只能再wrap一层
local function args_wrap(method_name, beg, ...)
    rpc:update_statistic(method_name, ev:real_ms_time() - beg)
    return ...
end

-- 收到rpc调用，由C++触发
function rpc_command_new(conn_id, rpc_id, method_name, ...)
    -- 把参数平铺到栈上，这样可以很方便地处理可变参而不需要创建一个table来处理参数，减少gc压力
    rpc.last_conn_id = conn_id
    local func = name_to_func[method_name]
    if not func then
        return error(string.format("rpc:[%s] was not declared", method_name))
    end

    if rpc.rpc_perf then
        return args_wrap(method_name, ev:real_ms_time(), func(...))
    else
        return func(...)
    end
end

-- 收到rpc调用，由C++触发
function rpc_command_return(conn_id, rpc_id, ecode, ...)
    rpc.last_conn_id = conn_id
    local callback = rpc.callback[rpc_id]
    if not callback then
        ERROR("rpc return no callback found:id = %d", rpc_id)
        return
    end
    rpc.callback[rpc_id] = nil

    return callback(ecode, ...)
end

return rpc
