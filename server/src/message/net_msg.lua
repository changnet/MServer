-- 网络消息
NetMsg = {}

local Buffer = require "engine.Buffer"
local PlayerQueue = require "queue.player_queue"

local buffer_read_int = Buffer.read_int
local buffer_write_int = Buffer.write_int
local buffer_write_buffer = Buffer.write_buffer

local callback = {} -- 消息回调
local LOCAL_TYPE = LOCAL_TYPE -- 当前worker的类型
local CLT_MSG_S = ThreadMessage.CLT_MSG_S
local CLT_MSG_C = ThreadMessage.CLT_MSG_C
local CLT_CAST  = ThreadMessage.CLT_CAST
local DEF_DISPATCH_WTYPE = W.PLAYER -- 默认为玩家所在worker，这个worker用的协议比较多

-- 广播子类型
local CAST_BROADCAST = 0 -- 全服广播
local CAST_MULTICAST = 1 -- 组播(指定pid列表)
local CAST_CHANNEL   = 2 -- 频道广播
local forward_wtype -- [id] = wtype

local g_mthread = g_mthread
local construct_message = g_mthread.construct_message

local pbc_decode = Pbc.decode
local pbc_encode = Pbc.encode

local IS_PLAYER = WORKER[LOCAL_TYPE].pobj
local IS_GATEWAY = LOCAL_TYPE == W.GATEWAY

local player_queue_push = PlayerQueue.push

local IS_PROFILE = true
local Profile = require "profile.profile"
if IS_PROFILE then
    Profile.init_timing_context("NetMsg", 256)
end

local function callback_factory(id, func)
    if not IS_PROFILE then return func end

    return function(...)
        Profile.begin_timing("NetMsg", true)
        func(...)
        Profile.end_timing("NetMsg", id)
    end
end

-- 注册网络消息回调
-- @param id 协议id
-- @param func 回调函数
function NetMsg.reg(msg, func)
    local wtype = WorkerNameType[msg.w] or DEF_DISPATCH_WTYPE
    if LOCAL_TYPE ~= wtype then return end

    local id = msg.i
    assert(not callback[id])
    callback[id] = {
        f = callback_factory(id, func),
        c = msg.c,
        s = msg.s,
    }
end

-- 注册不需要认证(未完成登录流程)的网络消息回调
-- @param id 协议id
-- @param func 回调函数
-- @param flag 认证flag，0表示未认证才可发送，1表示认证、未认证都可发送
function NetMsg.reg_noauth(msg, func, flag)
    local wtype = WorkerNameType[msg.w] or DEF_DISPATCH_WTYPE
    if LOCAL_TYPE ~= wtype then return end

    local id = msg.i
    assert(not callback[id])
    callback[id] = {
        f = callback_factory(id, func),
        c = msg.c,
        s = msg.s,
        n = flag or 0, -- noauth
    }
end

-- 网关加载用于转发的协议数据
function NetMsg.load_forward_msg()
    if LOCAL_TYPE ~= W.GATEWAY then return end

    forward_wtype = {}
    local wnt = WorkerNameType

    for _, msg in pairs(M) do
        local i = msg.i
        local wname = msg.w
        forward_wtype[i] = wnt[wname] or DEF_DISPATCH_WTYPE
    end
end

local function clt_msg_callback(func, schema, pid, buffer, size)
    local pkt = pbc_decode(schema, buffer, size)

    if IS_PLAYER then
        -- player用玩家对象回调，避免每次处理消息再获取一次
        local player = PlayerMgr.get_player(pid)
        if not player then
            -- 未登录成功，uninit_player中的玩家不接受前端协议
            eprint("clt_msg_callback no player", pid)
            return
        end
        return player_queue_push(player, func, player, pkt)
    else
        -- 其他的以pid回调，这些worker没有玩家对象，比如竞技场
        return CoPool.invoke(func, pid, pkt)
    end
end

-- 派发客户端网络消息
-- @param buffer 消息二进制数据
-- @param size 消息大小
function NetMsg.dispatch(socket, id, buffer, size)
    local pid = socket.pid
    local auth = socket.auth
    local wtype = forward_wtype[id]

    -- 在当前线程处理
    if LOCAL_TYPE == wtype then
        local cb = callback[id]
        if not cb then
            printf("message dispatch no callback for %d", id)
            return
        end

        local n = cb.n
        if auth then
             -- 已认证的，禁止发送不需要认证的消息
            if 0 == n then
                eprintf("%s socket auth for noauth message %d", socket.account, id)
                return
            end
            return clt_msg_callback(cb.f, cb.c, pid, buffer, size)
        else
            -- 未认证的，禁止发送需要认证后的消息
            if not n then
                eprintf("%s socket not auth for message %d", socket.account, id)
                return
            end
            local pkt = pbc_decode(cb.c, buffer, size)
            return cb.f(socket, pkt)
        end
    end

    -- 登录认证流程必须在网关完成，不能扩散到其他worker
    -- 如果指定协议的w字段错了，上面没有拦截，也会出现这个报错
    if not auth then
        eprintf("%s socket not auth for message %d", socket.account, id)
        return
    end

    -- 对于player scene等类型的worker，存在多个，需要根据玩家当前在哪个worker进行转发
    local addr = Router.find_player_addr(pid, wtype)
    local worker = WorkerHash[addr]
    if not worker then
        eprint("player message dispatch worker not found",
            socket.account, pid, wtype, addr)
        return
    end

    --[[
    方法1：在当前解析protobuf数据，用rpc直接丢到目标worker，简单粗暴，就是把protobuf
    解析出来又不用，浪费cpu和内存

    方法2：用二进制接口构建一个message，丢到对应的worker去，比较节省，但有点不安全。如果
    中途报错可能会导致内存泄漏。二进制操作一不小心就宕机。
    ]]

    local m, mbuffer = construct_message(
        g_mthread, LOCAL_ADDR, addr, CLT_MSG_S, size + 10)
    buffer_write_int(mbuffer, 8, pid) -- 玩家id
    buffer_write_int(mbuffer, 2, id, 8) -- 协议id
    buffer_write_buffer(mbuffer, buffer, size, 10) -- protobuf数据

    return worker:push_message(m)
end

-- 收到网关转发而来的客户端消息
local function dispatch_clt_message(src, udata, size)
    local pid = buffer_read_int(udata, 8)
    local id, pb = buffer_read_int(udata, 2, 8)
    local cb = callback[id]
    if not cb then
        printf("dispatch_clt_message no callback for %d", id)
        return
    end

    return clt_msg_callback(cb.f, cb.c, pid, pb, size - 10)
end

-- 以player对象发送消息到客户端
-- @param player table 玩家对象
-- @param cmd table 协议定义，包含i(协议id)和s(schema)
-- @param pkt table 协议数据
function NetMsg.send(player, cmd, pkt)
    local cmd_id = cmd.i
    local pid = player.pid
    local buffer, size = pbc_encode(cmd.s, pkt)

    -- 在网关直接发送，不需要走消息队列（网关现在没有player对象）
    -- if IS_GATEWAY then
    --     local socket = CltMgr.get_by_pid(pid)
    --     if not socket then
    --         eprint("NetMsg.send no socket", pid)
    --         return
    --     end
    --     return socket:send_pkt(cmd.i, buffer, size)
    -- end

    local gaddr = player.gaddr
    local worker = WorkerHash[gaddr]
    if not worker then
        eprint("NetMsg.send no gateway", pid, gaddr)
        return
    end

    local m, mbuffer = construct_message(
        g_mthread, LOCAL_ADDR, gaddr, CLT_MSG_C, size + 10)
    buffer_write_int(mbuffer, 8, pid)       -- 玩家id
    buffer_write_int(mbuffer, 2, cmd_id, 8)  -- 协议id
    buffer_write_buffer(mbuffer, buffer, size, 10) -- pb数据

    return worker:push_message(m)
end

-- 以socket对象发送消息到客户端，仅在网关可用
function NetMsg.send_socket(socket, cmd, pkt)
    local buffer, size = pbc_encode(cmd.s, pkt)
    return socket:send_pkt(cmd.i, buffer, size)
end

-- 以pid发送消息到客户端
-- @param pid number 玩家id
-- @param cmd table 协议定义，包含i(协议id)和s(schema)
-- @param pkt table 协议数据
function NetMsg.send_pid(pid, cmd, pkt)
    local cmd_id = cmd.i
    local buffer, size = pbc_encode(cmd.s, pkt)

    -- 在网关直接发送
    if IS_GATEWAY then
        local socket = CltMgr.get_by_pid(pid)
        if not socket then
            eprint("NetMsg.send_pid no socket", pid)
            return
        end
        return socket:send_pkt(cmd.i, buffer, size)
    end

    local gaddr = Router.find_player_addr(pid, W.GATEWAY)
    if not gaddr then
        eprint("NetMsg.send_pid no gateway addr", pid)
        return
    end

    local worker = WorkerHash[gaddr]
    if not worker then
        eprint("NetMsg.send_pid no gateway", pid, gaddr)
        return
    end

    local m, mbuffer = construct_message(
        g_mthread, LOCAL_ADDR, gaddr, CLT_MSG_C, size + 10)
    buffer_write_int(mbuffer, 8, pid)       -- 玩家id
    buffer_write_int(mbuffer, 2, cmd_id, 8)  -- 协议id
    buffer_write_buffer(mbuffer, buffer, size, 10) -- pb数据

    return worker:push_message(m)
end

-- 网关收到其他worker发来的需要转发给客户端的消息
local function dispatch_to_clt(src, udata, size)
    local pid = buffer_read_int(udata, 8)
    local id, pb = buffer_read_int(udata, 2, 8)

    local socket = CltMgr.get_by_pid(pid)
    if not socket then
        eprint("dispatch_to_clt no socket", pid)
        return
    end

    -- dprint("send clt msg", pid, id, size - 10)
    return socket:send_pkt(id, pb, size - 10)
end

-- 发送CLT_CAST到所有网关
-- @param total_size number 消息总大小
-- @param write_fn function(mbuffer) 写入消息内容的回调
local function send_cast_to_gateways(total_size, write_fn)
    local gw_list = Router.get_worker_list(W.GATEWAY)
    if not gw_list then return end

    for _, gw in ipairs(gw_list) do
        local worker = WorkerHash[gw.addr]
        if worker then
            local m, mbuffer = construct_message(
                g_mthread, LOCAL_ADDR, gw.addr,
                CLT_CAST, total_size)
            write_fn(mbuffer)
            worker:push_message(m)
        end
    end
end

-- 广播到所有进入游戏完成的玩家（在player和game可能成功，但网关未成功，以网关为准）
-- @param cmd table 协议定义，包含i(协议id)和s(schema)
-- @param pkt table 协议数据
function NetMsg.broadcast(cmd, pkt)
    local cmd_id = cmd.i
    local buffer, size = pbc_encode(cmd.s, pkt)
    local total = size + 4 -- 2(type) + 2(cmd_id)

    send_cast_to_gateways(total, function(mbuffer)
        buffer_write_int(mbuffer, 2, CAST_BROADCAST)
        buffer_write_int(mbuffer, 2, cmd_id, 2)
        buffer_write_buffer(mbuffer, buffer, size, 4)
    end)
end

-- 组播到指定玩家（在player和game可能成功，但网关未成功，以网关为准）
-- @param pids integer[] 目标玩家pid列表
-- @param cmd table 协议定义，包含i(协议id)和s(schema)
-- @param pkt table 协议数据
function NetMsg.multicast(pids, cmd, pkt)
    local cmd_id = cmd.i
    local buffer, size = pbc_encode(cmd.s, pkt)
    local pid_count = #pids
    -- 2(type) + 2(count) + 8*N(pids) + 2(cmd_id)
    local header = 4 + pid_count * 8 + 2
    local total = header + size

    send_cast_to_gateways(total, function(mbuffer)
        buffer_write_int(mbuffer, 2, CAST_MULTICAST)
        buffer_write_int(mbuffer, 2, pid_count, 2)
        local offset = 4
        for i = 1, pid_count do
            buffer_write_int(mbuffer, 8, pids[i], offset)
            offset = offset + 8
        end
        buffer_write_int(mbuffer, 2, cmd_id, offset)
        buffer_write_buffer(
            mbuffer, buffer, size, offset + 2)
    end)
end

-- 网关收到CLT_CAST广播/组播/频道消息
local function dispatch_cast(src, udata, size)
    local cast_type = buffer_read_int(udata, 2)

    if cast_type == CAST_BROADCAST then
        -- 全服广播：遍历所有已认证连接
        local cmd_id, pb = buffer_read_int(udata, 2, 2)
        local pb_size = size - 4
        local sockets = CltMgr.get_pid_sockets()
        for _, socket in pairs(sockets) do
            if socket.auth then
                socket:send_pkt(cmd_id, pb, pb_size)
            end
        end
    elseif cast_type == CAST_MULTICAST then
        -- 组播：遍历pid列表
        local pid_count = buffer_read_int(udata, 2, 2)
        local offset = 4
        local cmd_offset = offset + pid_count * 8
        local cmd_id, pb = buffer_read_int(
            udata, 2, cmd_offset)
        local pb_size = size - cmd_offset - 2
        for _ = 1, pid_count do
            local pid = buffer_read_int(
                udata, 8, offset)
            offset = offset + 8
            local socket = CltMgr.get_by_pid(pid)
            if socket and socket.auth then
                socket:send_pkt(cmd_id, pb, pb_size)
            end
        end
    elseif cast_type == CAST_CHANNEL then
        -- 频道广播：委托给Channel模块
        Channel._dispatch_broadcast(udata, size)
    else
        eprint("dispatch_cast unknown type", cast_type)
    end
end

ThreadMessage.reg(CLT_MSG_S, dispatch_clt_message)
if IS_GATEWAY then
    ThreadMessage.reg(CLT_MSG_C, dispatch_to_clt)
    ThreadMessage.reg(CLT_CAST, dispatch_cast)
end

return NetMsg
