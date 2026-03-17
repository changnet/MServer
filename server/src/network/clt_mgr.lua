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
    -- 顶号时，已经通知旧连接关闭，但可能还来不及关闭
    if this.pid_socket[pid] then
        print("player already have a socket, overwrite:", pid)
    end

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
    -- 关服时所有socket已经强制清掉
    if g_shuttingdown then return end

    local session_id = socket.session_id
    if not this.session[session_id] then
        print("client delete no socket found:", session_id, debug.traceback())
        return
    end

    local pid = socket.pid
    if pid then this.pid_socket[pid] = nil end

    this.session[session_id] = nil

    local info = socket.login
    if info then
        local addr = Router.find_worker_addr(W.ACCOUNT, info.account)
        Send.AccountMgr.logout(addr, session_id)
    end

    printf("client connect del: %d", session_id)
end

-- 关服清理数据
local function shutdown()
    print("client mgr shutdown, listen close")
    local socket = this.listen_socket
    if socket then
        socket:close()
        this.listen_socket = nil
    end

    for _, s in pairs(this.session) do
        s:close()
    end
    this.session = nil
end

-- 服务器启动完成才开启客户端监听
local function start_listen(retry)
    -- game那边启动完成，才会启动监听，免得玩家连上时，游戏数据都未初始化
    local w = WorkerHash[GAME_ADDR]
    if w and w:is_ready() then
        print("client mgr waitting for game worker...")
        return
    end

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

    Shutdown.reg({
        name = "clt_mgr",
        func = shutdown,
    })
    return true
end

Startup.reg(start_listen, 0xFFFFFFFF)

return CltMgr
