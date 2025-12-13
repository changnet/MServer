-- 管理服务器与客户端的连接
CltMgr = {}

local this = memory("CltMgr", {
    seed = 0, -- 生成session_id的种子
    session = {}, -- [session_id] = socket, 与客户端的连接，这里的连接可能未认证
    pid_socket = {}, -- [pid] = socket, 与客户端的连接
})

-- 主动关闭客户端连接(只关闭连接，不处理其他帐号下线逻辑，仅用于强制关服)
function CltMgr.close(socket)
    this.session[socket.session] = nil
    if socket.pid then this.pid_socket[socket.pid] = nil end

    socket:close()
end

-- 根据pid主动关闭客户端连接
function CltMgr.close_by_pid(pid)
    local socket = this.pid_socket[pid]
    if not socket then
        eprint("close_by_pid no socket found:%d", pid)
        return
    end

    CltMgr.close(socket)
end


-- 设置客户端连接
function CltMgr.bind(socket, pid)
    assert(nil == this.pid_socket[pid], "player already have a connection")

    socket.pid = pid
    this.pid_socket[pid] = socket
end

-- 获取客户端连接
function CltMgr.get_by_pid(pid)
    return this.pid_socket[pid]
end

-- 根据socket_id获取连接
function CltMgr.get_by_session_id(session_id)
    return this.session[session_id]
end


-- 新增客户端连接
function CltMgr.add(socket)
    local session_id, new_seed = Engine.make_safe_id(this.seed, this.session)
    this.seed = new_seed

    socket.session_id = session_id
    this.session[session_id] = socket

    printf("client connection add: %d", session_id)
end

-- 客户端连接断开回调
function CltMgr.del(socket)
    local session_id = socket.session_id
    local socket = this.session[session_id]
    AccMgr.role_offline(socket_id)

    this.clt_socket[socket_id] = nil
    -- 如果已经登录，通知其他服玩家下线
    if socket.pid then
        this.pid_socket[socket.pid] = nil
        local pkt = {pid = socket.pid}
        SrvMgr.send_world_pkt(SYS.PLAYER_OFFLINE, pkt)
    end

    printf("client connect del: %d", session_id)
end

-- 服务器启动完成才开启客户端监听
local function on_ready()
    local gateway = assert(g_setting.gateway[LOCAL_NAME])

    -- 开启监听
    local ScSocket = require "network.sc_socket"

    local socket = ScSocket()
    local ok = socket:listen(gateway.host, gateway.port)
    if not ok then
        eprint("listen client fail", socket:error())
        return
    end
    this.listen_socket = socket
    printf("listen client at %s:%d", gateway.host, gateway.port)
end

-- 关服清理数据
local function shutdown()
    local socket = this.listen_socket
    if socket then
        socket:close()
        this.listen_socket = nil
    end

    for _, clt_socket in pairs(this.clt_socket) do
        clt_socket:close()
    end
end

SE.reg(SE_READY, on_ready)
Shutdown.reg("ClgMgr", {
    shutdown = shutdown,
    ready = function() return true end
})

return CltMgr
