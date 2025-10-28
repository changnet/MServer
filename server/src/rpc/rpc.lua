-- 远程调用
Rpc = {}

-- rpc调用，使用协程阻塞返回
Call = {}

-- rpc调用，无返回
Send = {}

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
local g_mthread = g_mthread
local WorkerHash = WorkerHash
local RPC_REQ = ThreadMessage.RPC_REQ
local RPC_RES = ThreadMessage.RPC_RES
local lcodec_encode_to_buffer = g_lcodec.encode_to_buffer

-- 用parse_name_func而不是name_to_func，避免脚本报错时rtti失效导致rpc也失效了
local parse_name_func = Rtti.parse_name_func

local last_src -- 最后一次调用rpc的来源

function Rpc.set_metatable(module, factory_func)
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
            local func = factory_func(name)

            tbl[name] = func
            return func(...)
        end
    }

    setmetatable(module, mt)
end

-- 为了获取rpc返回的第一个参数，并实现在co内报错，需要wrap一层函数
local function call_return(ok, ...)
    if not ok then
        error("rpc remote error")
    end
    return ...
end

local function send_func_factory(name)
    return function(addr, ...)
        local w = WorkerHash[addr] or g_mthread

        local ptr, size = lcodec_encode_to_buffer(g_lcodec, 0, name, ...)
        return w:emplace_message(LOCAL_ADDR, addr, RPC_REQ, ptr, size)
    end
end

local function call_func_factory(name)
    return function(addr, ...)
        local w = WorkerHash[addr] or g_mthread

        -- 每个协程需要分配一个session，这样返回时才知道唤醒哪个协程
        local session = CoPool.current_session()
        local ptr, size = lcodec_encode_to_buffer(g_lcodec, session, name, ...)
        w:emplace_message(LOCAL_ADDR, addr, RPC_REQ, ptr, size)

        return call_return(coroutine.yield())
    end
end

local function do_request(src, session, name, ...)
    local func = parse_name_func(name)
    if not func then
        eprint("rpc no func found", name, LOCAL_ADDR)

        if 0 ~= session then
            local ptr, size = g_lcodec:encode_to_buffer(session, false)
            local w = WorkerHash[src] or g_mthread
            w:emplace_message(LOCAL_ADDR, src, RPC_RES, ptr, size)
        end
        return
    end

    last_src = src

    -- session表示不需要返回
    if 0 == session then
        return CoPool.invoke(func, ...)
    else
        local ptr, size = g_lcodec:encode_to_buffer(
            session, CoPool.invoke(func, ...))

        local w = WorkerHash[src] or g_mthread
        return w:emplace_message(LOCAL_ADDR, src, RPC_RES, ptr, size)
    end
end

local function request_dispatch(src, udata, usize)
    return do_request(src, g_lcodec:decode_from_buffer(udata, usize))
end

local function do_response(session, ...)
    return CoPool.resume(session, ...)
end

local function response_dispatch(src, udata, usize)
    last_src = src
    return do_response(g_lcodec:decode_from_buffer(udata, usize))
end

-- 获取上一次调用的来源
function Call.last_source()
    return last_src
end

-- 获取上一次调用的来源
function Send.last_source()
    return last_src
end

Rpc.call_return = call_return

Rpc.set_metatable(Call, call_func_factory)
Rpc.set_metatable(Send, send_func_factory)

ThreadMessage.reg(RPC_REQ, request_dispatch)
ThreadMessage.reg(RPC_RES, response_dispatch)

require "rpc.rpc_proxy"
