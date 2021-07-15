-- 管理与客户端的连接
local CltMgr = oo.singleton(...)

local ScConn = require "network.sc_conn"

--  监听客户端连接
function CltMgr:clt_listen(ip, port)
    self.clt_listen_conn = ScConn()
    self.clt_listen_conn:listen(ip, port)

    PRINTF("listen for client at %s:%d", ip, port)
    return true
end

-- 主动关闭客户端连接(只关闭连接，不处理其他帐号下线逻辑)
function CltMgr:clt_close(clt_conn)
    self.clt_conn[clt_conn.conn_id] = nil
    if clt_conn.pid then self.clt[clt_conn.pid] = nil end

    clt_conn:close()
end

-- 根据pid主动关闭客户端连接
function CltMgr:clt_close_by_pid(pid)
    local conn = self.clt[pid]
    if not conn then
        ERROR("clt_close_by_pid no conn found:%d", pid)
        return
    end

    self:clt_close(conn)
end


-- 设置客户端连接
function CltMgr:bind_role(pid, clt_conn)
    ASSERT(nil == self.clt[pid], "player already have a conn")

    clt_conn:bind_role(pid)
    self.clt[pid] = clt_conn
end

-- 获取客户端连接
function CltMgr:get_clt_conn(pid)
    return self.clt[pid]
end

-- 根据conn_id获取连接
function CltMgr:get_conn(conn_id)
    return self.clt_conn[conn_id]
end


-- 新增客户端连接
function CltMgr:clt_conn_accept(conn_id, conn)
    self.clt_conn[conn_id] = conn

    PRINTF("accept client connection:%d", conn_id)
end

-- 客户端连接断开回调
function CltMgr:clt_conn_del(conn_id)
    local conn = self.clt_conn[conn_id]
    g_account_mgr:role_offline(conn_id)

    self.clt_conn[conn_id] = nil
    -- 如果已经登录，通知其他服玩家下线
    if conn.pid then
        self.clt[conn.pid] = nil
        local pkt = {pid = conn.pid}
        g_srv_mgr:send_world_pkt(SYS.PLAYER_OFFLINE, pkt)
    end

    PRINTF("client connect del:%d", conn_id)
end

-- 此函数必须返回一个value为玩家id的table
-- CLTCAST定义在define.lua
function clt_multicast_new(mask, ...)
    if mask == CLTCAST.WORLD then
        local pid_list = {}
        for pid in pairs(g_clt_mgr.clt) do
            table.insert(pid_list, pid)
        end
        return pid_list
        -- elseif mask == CLTCAST.LEVEL then
    else
        ERROR("clt_multicast_new unknow mask", mask)
    end
end

local clt_mgr = CltMgr()
return clt_mgr
