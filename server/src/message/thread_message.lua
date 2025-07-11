-- 线程消息派发
ThreadMessage = {
    NONE    = 0, -- 无作用，只是唤醒线程
    TIMER   = 1, -- 定时器
    SIGNAL  = 2, -- 信号
    SOCKET  = 3, -- 网络socket数据
    RPC_REQ = 4, -- rpc请求
    RPC_RES = 5, -- rpc返回
    CLT_MSG = 6, -- 客户端消息
}

local LOCAL_ADDR = LOCAL_ADDR
local WorkerHash = WorkerHash
local type_dispatch = {}

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

-- 构建一个消息，并发送给目标地址
-- @param src 来源地址
-- @param dst 目标地址
-- @param mtype 消息类型
-- @param ptr 自定义数据
-- @param size 自定义数据长度
function ThreadMessage.emplace(src, dst, mtype, ptr, size)
    local w = WorkerHash[dst] or g_mthread
    return w:emplace_message(src or LOCAL_ADDR, dst, mtype, ptr, size)
end

-- 转发一个消息到目标地址（不影响原消息的所有权，数据会复制一份）
function ThreadMessage.forward(addr, msg)
    local _, _, mtype, ptr, size = g_mthread:unpack_message()
    local w = WorkerHash[addr] or g_mthread
    return w:emplace_message(LOCAL_ADDR, addr, mtype, ptr, size)
end

-- 转发一个消息到目标地址（夺取原消息的所有权，原线程不能销毁此消息，也不能再持有此消息)
-- @param msg 可以通过g_mthread.construct_message或者acquire_message获取
function ThreadMessage.transfer(addr, msg)
    local w = WorkerHash[addr] or g_mthread
    return w:push_message(msg)
end

-- 主线程回调函数(由C++调用)
function main_message_dispatch(src, dst, mtype, udata, usize)
    local worker = WorkerHash[dst]
    if worker then
        -- 这个慢一点
        -- return worker:emplace_message(src, dst, mtype, udata, usize)

        local m = g_mthread:acquire_message()
        return worker:push_message(m)
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
    return func(src, udata, usize)
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

local function func_none()
    -- 通常只用于唤醒线程，进入循环
end

ThreadMessage.reg(ThreadMessage.NONE, func_none)

return ThreadMessage
