-- 管理与客户端的连接
CltMgr = {}

local ScConn = require "network.sc_conn"
local this = global_storage("CltMgr", {
    clt = {}, -- 与客户端的连接，pid为key
    clt_conn = {}, -- 与客户端的连接，conn_id为key
})

-- 主动关闭客户端连接(只关闭连接，不处理其他帐号下线逻辑)
function CltMgr.clt_close(clt_conn)
    this.clt_conn[clt_conn.conn_id] = nil
    if clt_conn.pid then this.clt[clt_conn.pid] = nil end

    clt_conn:close()
end

-- 根据pid主动关闭客户端连接
function CltMgr.clt_close_by_pid(pid)
    local conn = this.clt[pid]
    if not conn then
        elog("clt_close_by_pid no conn found:%d", pid)
        return
    end

    CltMgr.clt_close(conn)
end


-- 设置客户端连接
function CltMgr.bind_role(pid, clt_conn)
    assert(nil == this.clt[pid], "player already have a conn")

    clt_conn:bind_role(pid)
    this.clt[pid] = clt_conn
end

-- 获取客户端连接
function CltMgr.get_clt_conn(pid)
    return this.clt[pid]
end

-- 根据conn_id获取连接
function CltMgr.get_conn(conn_id)
    return this.clt_conn[conn_id]
end


-- 新增客户端连接
function CltMgr.clt_conn_accept(conn)
    local conn_id = conn.conn_id
    this.clt_conn[conn_id] = conn

    printf("accept client connection:%d", conn_id)
end

-- 客户端连接断开回调
function CltMgr.clt_conn_del(conn_id)
    local conn = this.clt_conn[conn_id]
    AccMgr.role_offline(conn_id)

    this.clt_conn[conn_id] = nil
    -- 如果已经登录，通知其他服玩家下线
    if conn.pid then
        this.clt[conn.pid] = nil
        local pkt = {pid = conn.pid}
        SrvMgr.send_world_pkt(SYS.PLAYER_OFFLINE, pkt)
    end

    printf("client connect del:%d", conn_id)
end

-- 设置玩家当前所在场景session，用于协议自动转发
function CltMgr.set_player_session(pid, session)
    network_mgr:set_player_session(pid, session)
end


-- 此函数必须返回一个value为玩家id的table
-- CLTCAST定义在define.lua
function clt_multicast_new(mask, ...)
    if mask == CLTCAST.WORLD then
        local pid_list = {}
        for pid in pairs(this.clt) do
            table.insert(pid_list, pid)
        end
        return pid_list
        -- elseif mask == CLTCAST.LEVEL then
    else
        elog("clt_multicast_new unknow mask", mask)
    end
end

local function on_app_start(check)
    if check and this.clt_listen_conn then
        return this.clt_listen_conn.ok
    end

    -- 需要等待其他服务器初始化完成才开启客户端监听
    -- 不然服务器没初始化完成就有玩家进游戏了
    local wait = 0
    for name in pairs(APP) do
        for _, setting in pairs(g_setting[string.lower(name)] or {}) do
            if table.includes(setting.servers or {}, g_app.name) then
                wait = wait + 1
            end
        end
    end
    if wait > 0 then
        local srvs = SrvMgr.get_all_srv_conn()
        if table.size(srvs) < wait then return false end

        for _, srv in pairs(srvs) do
            if not srv.sync then return false end
        end
    end

    local ip = g_app_setting.cip
    local port = g_app_setting.cport

    -- 监听客户端连接
    this.clt_listen_conn = ScConn()
    local ok, msg = this.clt_listen_conn:listen(ip, port)

    if ok then
        printf("listen for client at %s:%d", ip, port)
        return true
    else
        printf("listen for client at %s:%d error: %s", ip, port, msg)
        return false
    end
end

if GATEWAY == APP_TYPE then
    g_app:reg_start("CltMgr", on_app_start, 30)
end

return CltMgr
