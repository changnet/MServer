-- 线程消息派发
-- 编号约定：0~63 给C++预留，64~127 给Lua预留，必须和 engine/src/thread/thread_context.hpp 一致
ThreadMessage = {
    -- 0~63：C++ 预留（前4个C++/Lua共用）
    NONE    = 0, -- 无作用，只是唤醒线程
    TIMER   = 1, -- 定时器
    SIGNAL  = 2, -- 信号
    SOCKET  = 3, -- 网络socket数据
    -- kcp新增（C++已占号，Lua只注册接收方）
    KCP_ACCEPT = 4, -- backend → worker：发现新客户端，请决定是否接入
    KCP_ADD    = 5, -- worker → backend：建会话
    KCP_DEL    = 6, -- worker → backend：删会话

    -- 64~127：Lua 预留
    RPC_REQ = 64, -- rpc请求      （原 4）
    RPC_RES = 65, -- rpc返回      （原 5）
    CLT_MSG_S = 66, -- 客户端发到服务端消息 （原 6）
    CLT_MSG_C = 67, -- 服务端发到客户端消息 （原 7）
    CLT_CAST  = 68, -- 广播/组播/频道消息   （原 8）
}

local EngineSocket = require "engine.Socket"
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
    local _, _, mtype, ptr, size = g_mthread:unpack_message(msg)
    local w = WorkerHash[addr] or g_mthread
    return w:emplace_message(LOCAL_ADDR, addr, mtype, ptr, size)
end

-- 转发一个消息到目标地址（夺取原消息的所有权，原线程不能销毁此消息，也不能再持有此消息)
-- @param msg 可以通过g_mthread.construct_message或者acquire_message获取
function ThreadMessage.transfer(addr, msg)
    local w = WorkerHash[addr] or g_mthread
    if not msg then assert(false) end
    return w:push_message(msg)
end

-- 主线程回调函数(由C++调用)
function main_message_dispatch(src, dst, mtype, udata, usize)
    local worker = WorkerHash[dst]
    if worker then
        -- 这个慢一点，需要丢掉当前message再创建一个
        -- return worker:emplace_message(src, dst, mtype, udata, usize)

        local m = g_mthread:acquire_message()
        if not m then assert(false) end
        return worker:push_message(m)
    end

    if 0 ~= dst and LOCAL_ADDR ~= dst then
        eprint("unknow main message address", dst, mtype)
        return
    end

    local func = type_dispatch[mtype]
    if not func then
        eprint("unknow main message type", mtype)
        return
    end

    -- 部分协议不需要以协程回调，如需要协程回调使用reg_co
    return func(src, udata, usize)
end

function cluster_message_dispatch(src, dst, mtype, udata, usize)
    local worker = WorkerHash[dst]
    if worker then
        -- 这里不能用acquire_message，因为消息是从socket解析出来的
        return worker:emplace_message(src, dst, mtype, udata, usize)
    end

    if 0 ~= dst and LOCAL_ADDR ~= dst then
        eprint("unknow cluster message address", dst, mtype)
        return
    end

    local func = type_dispatch[mtype]
    if not func then
        eprint("unknow cluster message type", mtype)
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

-- kcp新客户端接入通知（backend发现首包，由业务决定是否接入）
-- 已经park了首包，worker发KCP_ADD时backend会按序回放，不会丢包
ThreadMessage.reg(ThreadMessage.KCP_ACCEPT, function(_, udata, usize)
    local listen_id, addr, conv = EngineSocket.unpack_kcp_accept(udata, usize)
    if not listen_id then return end

    local srv = SocketMgr.get(listen_id)
    if srv then
        return srv:on_kcp_accept(addr, conv)
    end
    eprint("kcp accept no listen socket", listen_id)
end)

return ThreadMessage
