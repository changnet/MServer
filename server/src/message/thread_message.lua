-- 线程消息派发
ThreadMessage = {
    TIMER   = 1, -- 定时器
    SIGNAL  = 2, -- 信号
    SOCKET  = 3, -- 网络
    RPC_REQ = 4, -- rpc请求
    RPC_RES = 5, -- rpc返回
}

local worker_hash = {}
local type_dispatch = {}

-- 注册各个worker的

-- 注册消息回调
-- @param mtype 消息类型
function ThreadMessage.reg(mtype, func)
    type_dispatch[mtype] = func
end

-- 注册消息，以coroutine回调
-- @param mtype 消息类型
function ThreadMessage.reg_co(mtype, func)
    type_dispatch[mtype] = function(...) return CoPool.invoke(func, ...) end
end

-- 主线程回调函数(由C++调用)
function main_message_dispatch(src, dst, mtype, udata, usize)
    local worker = worker_hash[dst]
    if worker then
        return worker:emplace_message(src, dst, mtype, udata, usize)
    end

    if 0 ~= dst and LOCAL_ADDR ~= dst then
        eprint("unknow message address", dst, mtype)
        return
    end

    local func = type_dispatch[mtype]
    if not func then
        eprint("unknow message type", mtype)
        return
    end

    -- 部分协议不需要以协程回调，如需要协程回调使用reg_co
    func(src, udata, usize)
end

-- worker程回调函数(由C++调用)
function on_worker_message(src, mtype, udata, usize)
    local func = type_dispatch[mtype]
    if not func then
        eprint("worker unknow message type", mtype)
        return
    end

    return func(src, udata, usize)
end

return ThreadMessage
