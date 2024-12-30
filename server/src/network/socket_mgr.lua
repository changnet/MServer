-- Socket管理
SocketMgr = {
    -- 在C++定义，必须一致
    EV_READ      = 0x0001, -- 1  socket读(接收)
    EV_ACCEPT    = 0x0004, -- 4  监听到有新连接
    EV_CONNECT   = 0x0008, -- 8  连接成功或者失败
    EV_CLOSE     = 0x0010, -- 16 socket关闭
    EV_INIT_CONN = 0x0200, -- 512 初始化connect
    EV_INIT_ACPT = 0x0400, -- 1024 初始化accept
}

local EV_READ      = SocketMgr.EV_READ
local EV_ACCEPT    = SocketMgr.EV_ACCEPT
local EV_CONNECT   = SocketMgr.EV_CONNECT
local EV_CLOSE     = SocketMgr.EV_CLOSE
local EV_INIT_CONN = SocketMgr.EV_INIT_CONN
local EV_INIT_ACPT = SocketMgr.EV_INIT_ACPT


local LOCAL_ADDR = assert(LOCAL_ADDR)
local LOW_BIT = LOCAL_ADDR & 0xFFFF

__socket_hash = __socket_hash or {}
__socket_seed = __socket_seed or 0

-- 因为gc的延迟问题，用以弱表可能会导致取所有连接时不准确
-- setmetatable(self.conn, {["__mode"]='v'})

local __socket_hash = __socket_hash
local __socket_seed = __socket_seed

-- 分配当前进程唯一的socket id
function SocketMgr.next_id()
    -- 需要唯多个worker生成的id不会冲突
    -- 参考engine.lua中生成address的规则，低16位用作worker type和index
    -- 高16位用作自增，目前一个worker最多只能发起2^16=65535个连接
    local seed = __socket_seed

    local id
    for _ = 1, 0xFFFF do
        seed = seed + 1
        if seed > 0xFFFF then seed = 1 end

        id = (seed << 16) & LOW_BIT
        if not __socket_hash[id] then
            __socket_seed = seed
            return id
        end
    end

    assert(false, "no more id")
end

-- 新增socket
function SocketMgr.add()
end

-- 删除socket
function SocketMgr.del()
end

local function do_read(socket)
    local s = socket.s
    -- TODO 这里有点问题，假如这个socket比较忙，这里会一直获取到数据
    -- 其他socket的数据就没法处理了。所以每个socket只处理N个数据
    -- 如果还剩下数据需要post一个事件然后后续再处理
    while true do
        s:unpack()
    end
end

local function do_close(socket)
    socket.conn_ok = false
    local e = socket.s:close()
    __socket_hash[socket.conn_id] = nil

    socket:on_disconnected(e)
end

local function do_listen(socket)
    local s = socket.s
    while true do
        local fd = s:accept()
        if -1 == fd then return end
        if -2 == fd then error("listen socket error") end

        socket:conn_accept(fd)
    end
end

local function do_connect(socket)
    local e = socket.s:connect_validate()
    if 0 ~= e then
        socket.conn_ok = false
        socket.s:stop() -- not s:close()
        __socket_hash[socket.conn_id] = nil
    end
    socket:conn_new(e)
end

local function socket_dispatch(src, udata, id)
    local socket = __socket_hash[id]
    if not socket then
        print("socket dispatch no socket found", id)
        return
    end

    local revents = socket.s:get_event()
    --[[
    一个socket发送数据后立即关闭，则对端会同时收到EV_READ和EV_CLOSE
    这时的数据依然是有效的，所以EV_READ要优先EV_CLOSE来处理

    但EV_CLOSE的话，其他事件应该没有处理的必要了
    ]]
    if (EV_INIT_CONN & revents or EV_INIT_ACPT & revents) then
        -- 握手成功可能会直接收到消息，所以必须先回调握手成功
        socket.ok = true
        socket:io_ready()
    end
    if EV_READ & revents then
        do_read(socket)
    end
    if EV_CLOSE & revents then
        do_close(socket)
        return;
    end

    -- ACCEPT、CONNECT事件可能和READ、WRITE同时触发
    if EV_ACCEPT & revents then
        return do_listen(socket)
    elseif EV_CONNECT & revents then
        return do_connect(socket)
    end
end

ThreadMessage.reg(ThreadMessage.SOCKET, socket_dispatch)
