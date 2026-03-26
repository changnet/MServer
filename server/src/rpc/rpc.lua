-- 远程调用
Rpc = {}

-- rpc调用，使用协程阻塞返回
Call = {}

-- rpc调用，使用回调函数异步返回
Callback = {}

-- rpc调用，无返回
Send = {}

--[[
RPC设计

1. 版本1参考其他rpc库(如python xmlrpc: https://docs.python.org/3/library/xmlrpc.html)
   需要启动一个中心服务，注册所有函数。但这种方式很麻烦，要手动注册函数，还要考虑热更维护，起服同步
   调用方式为Rpc:func()，这种方式对代码提示也不友好

2. 版本2采用传入函数指针的方式，如Call(Test.func, 1, 2, 3)，Test.func就是远程函数
    这种方式在不同节点共用一份代码时，效果不错，代码工具可以直接跳转到Test.func
    但改成多个worker模式后，不同节点代码不一样，会出现要调用的函数根本没有引用的情况

3. 很多lua的rpc实现都是传字符串的，比如skynet.call(addr, name, ...))
    但传字符串会出现写代码无法跳转，无法提示的问题。
    并且只传字符串，还要在接收时按字符串处理。和古老的定协议号方式没什么区别，只是数字换成字符串


在经过几个版本方案的更换，也参考了其他语言的一些实现，确定了RPC的基本需求
1. 去中心化，不需要注册函数
2. 能直接调用函数而不需要注册回调
3. 有代码提示、跳转

对于1，lua可以根据字符串解析出函数，因此可以去中心化免注册
对于2，只需要稍微包装下通过字符串解析出函数，比如"Test.func"从全局获取Test模块，再取func函数即可
对于3，现有的工具都无法满足，只能通过Rpc.proxy封装，解决一部分问题

因此，新的方案采用Call.Test.func(addr, ...)这种方式，同时也保留Call.invoke(addr, Test.func, ...)方式，
也支持Call.invoke(addr, "Test.func", ...)这种方式

在适当的情况下，使用Rpc.proxy_wtype("Test", W.TEST)封装后，可以直接调用Test.func()


关于热更
    如果需要热更，使用RTTI模块是可以的。但实际应用过程中，单次rpc调用时间很短，
    不需要考虑热更，单次协程异步等待也不考虑热更
]]

local LuaCodec = require "engine.LuaCodec"
g_lcodec = g_lcodec or LuaCodec()

local g_lcodec = g_lcodec
local g_mthread = g_mthread
local WorkerHash = WorkerHash
local RPC_REQ = ThreadMessage.RPC_REQ
local RPC_RES = ThreadMessage.RPC_RES
local lcodec_encode_to_buffer = g_lcodec.encode_to_buffer

local func_to_name = Rtti.func_to_name
-- 用parse_name_func而不是name_to_func，避免脚本报错时rtti失效导致rpc也失效了
local parse_name_func = Rtti.parse_name_func

__rpc_session = __rpc_session or {seed = 1, session = {}}
local rpc_session = __rpc_session

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

    return setmetatable(module, mt)
end

-- 为了获取rpc返回的第一个参数，并实现在co内报错，需要wrap一层函数
local function call_return(ok, ...)
    if not ok then
        error("rpc remote error")
    end
    return ...
end

local function send(name, addr, ...)
    local w = WorkerHash[addr] or g_mthread

    local ptr, size = lcodec_encode_to_buffer(g_lcodec, 0, name, ...)
    return w:emplace_message(LOCAL_ADDR, addr, RPC_REQ, ptr, size)
end

local function send_func_factory(name)
    return function(addr, ...) return send(name, addr, ...) end
end

local function call(name, addr, ...)
    local w = WorkerHash[addr] or g_mthread

    -- 每个协程需要分配一个session，这样返回时才知道唤醒哪个协程
    local session = CoPool.current_session()
    local ptr, size = lcodec_encode_to_buffer(g_lcodec, session, name, ...)
    w:emplace_message(LOCAL_ADDR, addr, RPC_REQ, ptr, size)

    return call_return(coroutine.yield())
end

local function call_func_factory(name)
    return function(addr, ...) return call(name, addr, ...) end
end

local function next_id()
    local id = rpc_session.seed - 1
    if id < -0x7fffffff then id = -1 end

    rpc_session.seed = id
    if not rpc_session.session[id] then
        return id
    else
        return next_id()
    end
end

local function callback_func_factory(name)
    return function(addr, func, ...)
        local w = WorkerHash[addr] or g_mthread

        local session = next_id()
        rpc_session.session[session] = {
            func = func,
        }
        local ptr, size = lcodec_encode_to_buffer(g_lcodec, session, name, ...)
        return w:emplace_message(LOCAL_ADDR, addr, RPC_REQ, ptr, size)
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

    -- session为0表示不需要返回
    if 0 == session then
        return CoPool.invoke(func, ...)
    else
        -- 使用完成回调确保 func 完全执行完毕后才发送响应
        -- 当 func 内部有嵌套 RPC Call 时，协程中途 yield，
        -- 回调不会被触发；只有协程最终完成或出错时才发送响应
        return CoPool.invoke_with_completion(func, function(ok, ...)
            local ptr, size = g_lcodec:encode_to_buffer(
                session, ok, ...)
            local w = WorkerHash[src] or g_mthread
            w:emplace_message(LOCAL_ADDR, src, RPC_RES, ptr, size)
        end, ...)
    end
end

local function request_dispatch(src, udata, usize)
    return do_request(src, g_lcodec:decode_from_buffer(udata, usize))
end

local function do_response(session, ...)
    if session > 0 then
        return CoPool.resume(session, ...)
    end

    -- 异步回调方式
    local info = rpc_session.session[session]
    if not info then
        eprint("rpc no callback found", session)
        return
    end
    rpc_session.session[session] = nil
    return CoPool.invoke(info.func, ...)
end

local function response_dispatch(src, udata, usize)
    return do_response(g_lcodec:decode_from_buffer(udata, usize))
end

-- 根据函数指针直接调用函数
-- @param func 函数指针，或者直接传入函数名
function Call.invoke(addr, func, ...)
    local name = func_to_name(func)
    if not name then
        if "string" ~= type(func) then
            name = func
        else
            assert(false, "no func name")
        end
    end

    return call(name, addr, ...)
end

-- 根据函数指针直接调用函数
-- @param func 函数指针，或者直接传入函数名
function Send.invoke(addr, func, ...)
    local name = func_to_name(func)
    if not name then
        if "string" ~= type(func) then
            name = func
        else
            assert(false, "no func name")
        end
    end

    return send(name, addr, ...)
end

Rpc.send = send
Rpc.call = call
Rpc.call_return = call_return

Rpc.set_metatable(Call, call_func_factory)
Rpc.set_metatable(Send, send_func_factory)
Rpc.set_metatable(Callback, callback_func_factory)

ThreadMessage.reg(RPC_REQ, request_dispatch)
ThreadMessage.reg(RPC_RES, response_dispatch)

require "rpc.rpc_proxy"
