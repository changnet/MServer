-- 网络消息
Message = {}

local Buffer = require "engine.Buffer"

local buffer_read_int = Buffer.read_int
local buffer_write_int = Buffer.write_int
local buffer_write_buffer = Buffer.write_buffer

local callback = {} -- 消息回调
local local_type = LOCAL_TYPE -- 当前worker的类型
local CLT_MSG = ThreadMessage.CLT_MSG
local PLAYER = WORKER.PLAYER[1]

local g_mthread = g_mthread
local construct_message = g_mthread.construct_message

-- 注册网络消息回调
-- @param id 协议id
-- @param func 回调函数
-- @param wtype 仅在该类型的worker上回调
function Message.reg(id, func, wtype)
    if local_type ~= wtype then return end

    assert(not callback[id])
    callback[id] = {
        f = func,
        w = wtype
    }
end

-- 注册不需要认证(未完成登录流程)的网络消息回调
-- @param id 协议id
-- @param func 回调函数
-- @param wtype 仅在该类型的worker上回调
-- @param flag 认证flag，0表示未认证才可发送，1表示认证、未认证都可发送
function Message.reg_noauth(id, func, wtype, flag)
    if local_type ~= wtype then return end

    assert(not callback[id])
    callback[id] = {
        f = func,
        w = wtype,
        n = flag or 0, -- noauth
    }
end

local function do_callback(func, pid, buffer, size)
    local pkt = protobuf.decode(buffer, size)

    if PLAYER == local_type then
        -- player用玩家对象回调，避免每次处理消息再获取一次
        return func(player, pkt)
    else
        -- 其他的以pid回调，这些worker没有玩家对象，比如竞技场
        return func(pid, pkt)
    end
end

-- 派发客户端网络消息
-- @param buffer 消息二进制数据
-- @param size 消息大小
function Message.dispatch(socket, id, buffer, size)
    local cb = callback[id]
    if not cb then
        printf("message dispatch no callback for %d", id)
        return
    end

    -- 未认证的，禁止发送需要认证后的消息
    -- 已认证的，禁止发送不需要认证的消息
    local n = cb.n
    local auth = socket.auth
    if not auth and not n then
        eprintf("%s socket not auth for message %d", socket.account, id)
        return
    elseif auth and 0 == n then
        eprintf("%s socket auth for noauth message %d", socket.account, id)
        return
    end

    local wtype = cb.w
    local pid = socket.pid
    if local_type == wtype then
        return do_callback(cb.f, pid, buffer, size)
    end

    -- 对于player scene等类型的worker，存在多个，需要根据玩家当前在哪个worker进行转发
    local addr = WorkerRoute.get_player_addr(pid, wtype)
    local worker = WorkerHash[addr]
    if not worker then
        eprint("player worker not found", socket.account, pid, addr)
        return
    end

    --[[
    方法1：在当前解析protobuf数据，用rpc直接丢到目标worker，简单粗暴，就是把protobuf
    解析出来又不用，浪费cpu和内存

    方法2：用二进制接口构建一个message，丢到对应的worker去，比较节省，但有点不安全。如果
    中途报错可能会导致内存泄漏。二进制操作一不小心就宕机。
    ]]

    local m, mbuffer = construct_message(g_mthread, LOCAL_ADDR, addr, CLT_MSG, size)
    buffer_write_int(mbuffer, 8, pid)
    buffer_write_int(mbuffer, 2, id, 8)
    buffer_write_buffer(mbuffer, buffer, size, 10)

    -- 注意： buffer里是包含cmd的
    worker:push_message(m)

    -- 如果是本进程的worker，直接持有创建的message。远程节点则在写入网络后销毁
    if worker.cluster_worker then g_mthread:destruct_message(m) end
end

-- 收到网关转发而来的客户端消息
local function dispatch_clt_message(src, udata, size)
    local pid = buffer_read_int(udata, 8)
    local id, pb = buffer_read_int(udata, 2, 8)
    local cb = callback[id]
    if not cb then
        printf("dispatch_clt_message callback for %d", id)
        return
    end

    return do_callback(cb.f, pid, pb, size - 8 - 2)
end

ThreadMessage.reg(CLT_MSG, dispatch_clt_message)

return Message
