-- Socket管理
SocketMgr = {
    -- 在C++定义，必须一致
    EV_READ      = 0x0001, -- 1  socket读(接收)
    EV_ACCEPT    = 0x0004, -- 4  监听到有新连接
    EV_CONNECT   = 0x0008, -- 8  连接成功或者失败
    EV_CLOSE     = 0x0010, -- 16 socket关闭
    EV_INIT_CONN = 0x0200, -- 512 初始化connect
    EV_INIT_ACPT = 0x0400, -- 1024 初始化accept

    PC_ERROR   = 1, -- 出错
    PC_MORE    = 2, -- 数据不足
    PC_DATA    = 3, -- 数据
    PC_UPGRADE = 4, -- websocket的upgrade
    PC_CTRL    = 5, -- websocket的控制包

    -- socket.status
    OPENING = 1, -- 已发起连接，但尚未连接完成
    OPENED  = 2, -- 连接完成（包括握手等），可以发送消息
    CLOSING = 3, -- 已发起关闭，但未关闭完成
    CLOSED  = 4, -- 已关闭
}

local EV_READ      = SocketMgr.EV_READ
local EV_ACCEPT    = SocketMgr.EV_ACCEPT
local EV_CONNECT   = SocketMgr.EV_CONNECT
local EV_CLOSE     = SocketMgr.EV_CLOSE
local EV_INIT_CONN = SocketMgr.EV_INIT_CONN
local EV_INIT_ACPT = SocketMgr.EV_INIT_ACPT
local ACCEPT_CONNECT = EV_INIT_CONN | EV_INIT_ACPT

local PC_ERROR = SocketMgr.PC_ERROR
local PC_MORE = SocketMgr.PC_MORE
local PC_DATA = SocketMgr.PC_DATA
local PC_UPGRADE = SocketMgr.PC_UPGRADE

local CLOSED = SocketMgr.CLOSED

local LOCAL_ADDR = assert(LOCAL_ADDR)
local LOW_BIT = LOCAL_ADDR & 0xFFFF

__socket_hash = __socket_hash or {}
__socket_seed = __socket_seed or 0

-- 因为gc的延迟问题，用以弱表可能会导致取所有连接时不准确
-- setmetatable(self.conn, {["__mode"]='v'})

local __socket_hash = __socket_hash

-- 分配当前进程唯一的socket id
function SocketMgr.next_id()
    -- 需要唯多个worker生成的id不会冲突
    -- 参考engine.lua中生成address的规则，低16位用作worker type和index
    -- 高16位用作自增，目前一个worker最多只能发起2^16=65535个连接
    local seed = __socket_seed

    for _ = 1, 0xFFFF do
        seed = seed + 1
        if seed > 0xFFFF then seed = 1 end

        local id = (seed << 16) | LOW_BIT
        if not __socket_hash[id] then
            __socket_seed = seed
            return id
        end
    end

    assert(false, "no more id")
end

-- 根据连接id获取对象
function SocketMgr:get(socket_id)
    return __socket_hash[socket_id]
end

-- 新增socket
function SocketMgr.add(socket)
    __socket_hash[socket.socket_id] = socket
end

-- 删除socket
function SocketMgr.del(socket)
    __socket_hash[socket.socket_id] = nil
end

-- 处理消息回调
-- @return 是否继续读取消息
local function do_message(socket, code, ...)
    if PC_DATA == code then
        -- TODO 读写事件并不代码收到的消息完整，因此要确认消息完整后才使用协程回调
        -- 但这样的话，也导致了每个消息一个协程，效率是不是有点低
        CoPool.invoke(socket.on_message, socket, ...)
        return true
    elseif PC_MORE == code then
        return false -- 数据不足，需要更多数据才能解析
    elseif PC_UPGRADE == code then
        CoPool.invoke(socket.on_handshake, socket, ...)
        return true
    elseif PC_ERROR == code then
        print("socket message error", socket.socket_id, ...)
        socket:close()
        return false
    else
        print("socket unknow message code", code)
        return false
    end
end

local function do_read(socket)
    local s = socket.s
    -- 假如这个socket比较忙，这里会一直获取到数据
    -- 其他socket的数据就没法处理了。所以每个socket只处理N个数据
    -- 如果还剩下数据需要post一个事件然后后续再处理
    for _ = 1, 256 do
        if not do_message(socket, s:unpack()) then
            return
        end
    end

    -- 还有消息，延后处理
    s:set_event(EV_READ)
    g_worker:emplace_message(0, 0, ThreadMessage.SOCKET, socket.socket_id)
end

local function do_close(socket)
    -- 这时候fd并没有关闭，部分逻辑还需要用到fd，因此在close之前调用on_disconnected
    -- 如果socket.status不为closing，则为对方关闭链接
    CoPool.invoke(socket.on_disconnected, socket)

    socket.status = CLOSED
    local e = socket.s:close()
    __socket_hash[socket.socket_id] = nil

    if 0 ~= e then
        print("socket close, error code", socket.socket_id, e)
    end
end

local function do_listen(socket)
    local s = socket.s
    while true do
        local fd, e = s:accept()
        if -1 == fd then
            if not e then return end

            -- 这个报错是accept报错，可能是新的fd报错，并不一定是listen这个socket的报错
            -- 比如ENFILE(Too many open files)，因此暂时不关闭，触发错误日志等待手动处理
            eprint("socket do_listen error", e)
            return
        end

        CoPool.invoke(socket.on_accepting, socket, fd)
    end
end

local function do_connect(socket)
    local e = socket.s:connect_validate()
    if 0 ~= e then
        socket:close() -- not s:close()
    end

    return CoPool.invoke(socket.on_connecting, socket, e)
end

local function socket_dispatch(src, udata, id)
    local socket = __socket_hash[id]
    if not socket then
        print("socket dispatch no socket found", id)
        return
    end

    -- 因为socket的设计是读写线程共用一个事件字段，多次事件直接叠加在同一个字段而不是发多个message
    -- 所以事件是存在socket上而不放在message上
    local revents = socket.s:get_event()

    --[[
    一个socket发送数据后立即关闭，则对端会同时收到EV_READ和EV_CLOSE
    这时的数据依然是有效的，所以EV_READ要优先EV_CLOSE来处理

    但EV_CLOSE的话，其他事件应该没有处理的必要了
    ]]
    if 0 ~= (ACCEPT_CONNECT & revents) then
        -- 握手成功可能会直接收到消息，所以必须先回调握手成功
        socket.ok = true
        socket:io_ready() -- 这里不要return，可能同时还有read事件
    end
    if 0 ~= (EV_READ & revents) then
        do_read(socket) -- 这里不要return，可能同时还有close事件
    end
    if 0 ~= (EV_CLOSE & revents) then
        return do_close(socket)
    end

    -- ACCEPT、CONNECT事件可能和READ、WRITE同时触发
    if 0 ~= (EV_ACCEPT & revents) then
        return do_listen(socket)
    elseif 0 ~= (EV_CONNECT & revents) then
        return do_connect(socket)
    end
end

ThreadMessage.reg(ThreadMessage.SOCKET, socket_dispatch)
