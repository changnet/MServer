-- rpc调用
Rpc = {}

--[[
1. 版本1参考其他rpc库(如python xmlrpc: https://docs.python.org/3/library/xmlrpc.html)
   做过一个需要注册的rpc，调用时能直接通过函数名调用，并且自动识别目标链接
   如 rpc:foo() 中的foo是注册时生成的函数名，并且自动发往注册的链接

   但这有几个问题
   1. 注册过程繁琐，包括热更、多进程同名(如多个场景进程)
   2. 代码提示、跳转不正常

2. 版本2采用传入函数指针的方式。但这种方式在改成多个worker模式后，很难取到函数指针了，因此放弃

3. 版本3回归Rpc.Remote.func这种方式，但不需要注册，而是通过元表获取函数名。同时加入协程
不用考虑回调函数的问题了

关于热更
    回调函数直接存的指针，因此是不能热更的，但是单次rpc调用时间很短，也不需要考虑热更
    协程的话，也不考虑热更
]]

local LuaCodec = require "engine.LuaCodec"
g_lcodec = g_lcodec or LuaCodec()

local g_lcodec = g_lcodec
local WorkerHash = WorkerHash
local RPC_REQ = ThreadMessage.RPC_REQ
local RPC_RES = ThreadMessage.RPC_RES

local name_to_func = name_to_func
local func_to_name = func_to_name

local function send_func_factory(name)
    return function(addr, ...)
        local w = WorkerHash[addr]
        if not w then
            printf("rpc send no worker(%d) found: %s", addr, debug.traceback())
            return
        end
        local ptr = g_lcodec:encode_to_message(0, name, ...)
        return w:emplace_message_ptr(addr, RPC_REQ, ptr)
    end
end

local mt
mt =
{
    __index = function(tbl, k)
		local name = rawget(tbl, "__name")
		if name then
			name = name .. "." .. k
		else
			name = k
		end
        local mod = setmetatable({__name = name}, mt)

        tbl[k] = mod
        return mod
    end,
    __call = function(tbl, ...)
        local name = assert(rawget(tbl, "__name"))
        local func = send_func_factory(name)

        tbl[name] = func
        return func(...)
    end
}

-- 设置Rpc表的元表
setmetatable(Rpc, mt)

local function do_request(session, name, ...)
    local func = name_to_func[name]
    if not func then
        print("rpc no func found", name)
        return
    end

    -- session表示不需要返回
    if 0 == session then
        return func(...)
    else
        local ptr = g_lcodec:encode_to_message(func(...))
        return g_engine:emplace_message_ptr(addr, RPC_RES, ptr)
    end
end

local function request_dispatch(msg)
    return do_request(g_lcodec:decode_from_message(msg))
end

local function response_dispatch()
end

ThreadMessage.reg(RPC_REQ, request_dispatch)
ThreadMessage.reg(RPC_RES, response_dispatch)

return Rpc
