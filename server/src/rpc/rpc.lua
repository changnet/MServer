-- rpc调用
local Rpc = oo.singleton(...)

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

local name_to_func = name_to_func
local func_to_name = func_to_name

local name_to_obj = name_to_obj
local obj_to_name = obj_to_name

local stat = g_stat_mgr:get("cmd")
local AutoId = require "modules.system.auto_id"

function Rpc:__init()
    self.callback = {}

    self.auto_id = AutoId()

    self.last_e = 0 -- 上一次rpc返回的错误码

    self.stat = {}
    self.stat_tm = ev:time()
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

-- 获取上一次rpc回调、返回的连接
function Rpc:last_conn()
    return g_srv_mgr:get_conn(self.last_conn_id)
end

-- 上一次rpc返回的错误码
function Rpc:last_error()
    return self.last_e
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
        -- 唉，这样不行哎。当后面有其他参数时，unpack只返回第一个参数
        -- return cb(table.unpack(args), ...)
        local n = args.n
        local count = select("#", ...)
        for i = 1, count do
            -- args[n + i] = select(i, ...)
            rawset(args, n + i, select(i, ...))
        end

        -- table.pack打包的参数中允许包含nil
        -- 但当 args[n + i] = xx或者 rawset 中包含nil时，unpack无法解包出nil
        -- 这时候需要强制指定unpack的后两个参数
        return cb(table.unpack(args, 1, n + count))
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
-- @param func 需要调用的函数
-- @param ... 参数
function Rpc:conn_call(conn, func, ...)
    -- 如果不需要回调，则call_id为0
    local call_id = 0
    if self.next_cb_func then
        call_id = self.auto_id:next_id(self.callback)
        self.callback[call_id] = self.next_cb_func

        self.next_cb_func = nil
    end

    local name = func_to_name(func)

    return conn:send_rpc_pkt(call_id, name, ...)
end

-- 为了实现可变参数不用table.pack，只能再wrap一层
local function args_wrap(method_name, beg, ...)
    -- stat:update_statistic(method_name, ev:real_ms_time() - beg)
    return ...
end

-- 收到其他服的玩家调用
local function on_call(method_name, ...)
    local func = name_to_func(method_name)
    if not func then
        return error(string.format("rpc:[%s] was not declared", method_name))
    end

    if stat then
        return args_wrap(method_name, ev:real_ms_time(), func(...))
    else
        return func(...)
    end
end

-- 收到其他服的玩家调用
local function on_pid_call(pid, method_name, ...)
    local player = g_player_mgr:get_player(pid)
    if not player then
        PRINTF("on pid call player not found: %d %s", pid, method_name)
        return
    end

    return on_call(method_name, player, ...)
end

-- 通过玩家pid发起world进程函数调用，回调第一个参数为player对象
-- 当玩家不在线时，该调用将被丢弃
-- @param pid 玩家pid
-- @param func 需要调用的函数
-- @param ... 参数
function Rpc:pid_call(pid, func, ...)
    return self:call(GSE, on_pid_call, pid, func_to_name(func), ...)
end

-- 收到其他服的实体调用
local function on_entity_call(pid, method_name, ...)
    local player = g_entity_mgr:get_player(pid)
    if not player then
        PRINTF("on entity call player not found: %d %s", pid, method_name)
        return
    end

    return on_call(method_name, player, ...)
end

-- 通过玩家pid发起场景进程函数调用，回调第一个参数为entity_player对象
-- 当玩家不在该进程时，该调用将被丢弃
-- @param pid 玩家pid
-- @param func 需要调用的函数
-- @param ... 参数
function Rpc:entity_call(pid, func, ...)
    local session = network_mgr:set_player_session(pid)
    return self:call(session, on_entity_call, pid, func_to_name(func), ...)
end

-- 收到其他服的实体调用
local function on_object_call(obj_name, method_name, ...)
    local obj = name_to_obj(obj_name)
    if not obj then
        PRINTF("on object call obj not found: %d %s", obj_name, method_name)
        return
    end

    return on_call(method_name, obj, ...)
end

-- 通过全局对象函数调用，回调第一个参数为该对象(两边的进程都需要创建该全局对象)
-- @param obj 全局对象
-- @param func 需要调用的函数
-- @param ... 参数
function Rpc:obj_call(session, obj, func, ...)
    local name = obj_to_name(obj)
    if not name then
        error("on obj call name not found")
        return
    end

    return self:call(GSE, on_object_call, name, func_to_name(func), ...)
end

-- 通过session执行rpc调用
-- @param session 进程的session，如GATEWAY
-- @param func 需要调用的函数
-- @param ... 参数
function Rpc:call(session, func, ...)
    local conn = g_srv_mgr:get_conn_by_session(session)
    if not conn then
        PRINTF("rpc call no connection found: %d", session)
        return
    end

    return self:conn_call(conn, func, ...)
end

-- 几个特殊的函数，名字短一点以减少传输数据量
reg_func("__1", on_pid_call)
reg_func("__2", on_entity_call)
reg_func("__3", on_object_call)

-- /////////////////////////////////////////////////////////////////////////////
-- RPC基础功能在上面处理
-- /////////////////////////////////////////////////////////////////////////////

local rpc = Rpc()

-- 收到rpc调用，由C++触发
function rpc_command_new(conn_id, rpc_id, method_name, ...)
    -- 把参数平铺到栈上，这样可以很方便地处理可变参而不需要创建一个table来处理参数，减少gc压力
    rpc.last_conn_id = conn_id
    if "__1" == method_name then
        return on_pid_call(...)
    elseif "__2" == method_name then
        return on_entity_call(...)
    elseif "__3" == method_name then
        return on_entity_call(...)
    end

    return on_call(method_name, ...)
end

-- 收到rpc调用，由C++触发
function rpc_command_return(conn_id, rpc_id, e, ...)
    rpc.last_e = e
    rpc.last_conn_id = conn_id
    local callback = rpc.callback[rpc_id]
    if not callback then
        ERROR("rpc return no callback found:id = %d", rpc_id)
        return
    end
    rpc.callback[rpc_id] = nil

    return callback(...)
end

return rpc
