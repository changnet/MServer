-- 管理与客户端的连接
CltMgr = {}

local this = global_storage("CltMgr", {
    clt = {}, -- 与客户端的连接，pid为key
    clt_conn = {}, -- 与客户端的连接，socket_id为key
})

-- 主动关闭客户端连接(只关闭连接，不处理其他帐号下线逻辑)
function CltMgr.close(conn)
    this.clt_conn[conn.socket_id] = nil
    if conn.pid then this.clt[conn.pid] = nil end

    conn:close()
end

-- 根据pid主动关闭客户端连接
function CltMgr.close_by_pid(pid)
    local conn = this.clt[pid]
    if not conn then
        eprint("close_by_pid no conn found:%d", pid)
        return
    end

    CltMgr.close(conn)
end


-- 设置客户端连接
function CltMgr.bind(pid, clt_conn)
    assert(nil == this.clt[pid], "player already have a connection")

    clt_conn:bind_role(pid)
    this.clt[pid] = clt_conn
end

-- 获取客户端连接
function CltMgr.get_by_pid(pid)
    return this.clt[pid]
end

-- 根据socket_id获取连接
function CltMgr.get(socket_id)
    return this.clt_conn[socket_id]
end


-- 新增客户端连接
function CltMgr.add(conn)
    local socket_id = conn.socket_id
    this.clt_conn[socket_id] = conn

    printf("client connection add: %d", socket_id)
end

-- 客户端连接断开回调
function CltMgr.del(socket_id)
    local conn = this.clt_conn[socket_id]
    AccMgr.role_offline(socket_id)

    this.clt_conn[socket_id] = nil
    -- 如果已经登录，通知其他服玩家下线
    if conn.pid then
        this.clt[conn.pid] = nil
        local pkt = {pid = conn.pid}
        SrvMgr.send_world_pkt(SYS.PLAYER_OFFLINE, pkt)
    end

    printf("client connect del: %d", socket_id)
end

return CltMgr
