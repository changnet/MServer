-- 管理与服务器之间的连接
local SrvMgr = oo.singleton(...)

local SrvConn = require "network.srv_conn"

--  监听服务器连接
function SrvMgr:srv_listen(ip, port)
    self.srv_listen_conn = SrvConn()
    self.srv_listen_conn:listen(ip, port)

    PRINTF("listen for server at %s:%d", ip, port)
    return true
end


-- 主动连接其他服务器
function SrvMgr:connect_srv(srvs)
    for _, srvName in pairs(srvs) do
        local appSetting = g_setting[srvName]
        local conn = SrvConn()

        conn.auto_conn = true
        local conn_id = conn:connect(appSetting.sip, appSetting.sport)

        self.srv_conn[conn_id] = conn
        PRINTF("connect to %s at %s:%d",
            srvName, appSetting.sip, appSetting.sport)
    end
end

-- 重新连接到其他服务器
function SrvMgr:reconnect_srv(conn)
    local conn_id = conn:reconnect()

    self.srv_conn[conn_id] = conn
    PRINTF("reconnect to %s:%d", conn.ip, conn.port)
end


-- 服务器认证
function SrvMgr:srv_register(conn, pkt)
    local auth = util.md5(SRV_KEY, pkt.timestamp, pkt.session)
    if pkt.auth ~= auth then
        ERROR("SrvMgr:srv_register fail,session %d", pkt.session)
        return false
    end

    local _, _, id = g_app:decode_session(pkt.session)
    if id ~= tonumber(g_app.id) then
        ERROR("SrvMgr:srv_register id not match,expect %s,got %d", g_app.id,
              id)
        return
    end

    if self.srv[pkt.session] then
        ERROR("SrvMgr:srv_register session conflict:%d", pkt.session)
        return false
    end

    self.srv[pkt.session] = conn
    network_mgr:set_conn_session(conn.conn_id, pkt.session)
    return true
end

-- 检查一个连接超时
local tm_pkt = {response = true}
function SrvMgr:check_one_timeout(srv_conn, check_time)
    if not srv_conn.ok then return end -- 还没连接成功的不检查

    local ts = srv_conn:check(check_time)
    if ts > SRV_ALIVE_TIMES then
        PRINTF("%s server timeout", srv_conn:conn_name())

        self:close_srv_conn(srv_conn)
        if srv_conn.auto_conn then self.srv_waiting[srv_conn] = 1 end
    elseif ts > 0 and srv_conn.auth then
        srv_conn:send_pkt(SYS.BEAT, tm_pkt)
    end
end

-- 定时器回调
function SrvMgr:do_timer()
    -- 重连
    local waiting = self.srv_waiting
    if not table.empty(waiting) then self.srv_waiting = {} end
    for conn in pairs(waiting) do self:reconnect_srv(conn) end

    local check_time = ev:time() - SRV_ALIVE_INTERVAL
    for _, srv_conn in pairs(self.srv_conn) do
        self:check_one_timeout(srv_conn, check_time)
    end
end

-- 获取服务器连接
function SrvMgr:get_conn_by_session(session)
    return self.srv[session]
end

-- 获取所有服务器连接
function SrvMgr:get_all_srv_conn()
    return self.srv
end


-- 获取网关连接(在非网关服务器进程获取)
function SrvMgr:get_gateway_conn()
    return self.srv[gateway_session]
end

-- 在非网关进程直接发送数据到客户端
function SrvMgr:send_clt_pkt(pid, cmd, pkt, ecode)
    local srv_conn = self.srv[gateway_session]
    return srv_conn:send_clt_pkt(pid, cmd, pkt, ecode)
end

-- 发送服务器数据包到网关(现在同一类型只有一个进程才能这样做)
function SrvMgr:send_gateway_pkt(cmd, pkt, ecode)
    local srv_conn = self.srv[gateway_session]

    return srv_conn:send_pkt(cmd, pkt, ecode)
end

-- 发送服务器数据包到世界服(现在同一类型只有一个进程才能这样做)
function SrvMgr:send_world_pkt(cmd, pkt, ecode)
    local srv_conn = self.srv[world_session]

    return srv_conn:send_pkt(cmd, pkt, ecode)
end


-- 根据conn_id获取连接
function SrvMgr:get_conn(conn_id)
    return self.srv_conn[conn_id]
end

-- 主动关闭服务器链接
function SrvMgr:close_srv_conn(conn)
    conn:close()
    self:set_conn(conn.conn_id, nil)

    if conn.session then self.srv[conn.session] = nil end
    self.srv_conn[conn.conn_id] = nil
end

-- 服务器广播
function SrvMgr:srv_multicast(cmd, pkt, ecode)
    local conn_list = {}
    for _, conn in pairs(self.srv) do table.insert(conn_list, conn.conn_id) end
    return network_mgr:srv_multicast(conn_list, network_mgr.CDC_PROTOBUF, cmd.i,
                                     ecode or 0, pkt)
end

-- 客户端广播
-- 非网关向客户端广播，由网关转发.网关必须处理clt_multicast_new回调
-- @mask:掩码，参考C++宏定义clt_multicast_t。
-- @args_list:参数列表，根据掩码，这个参数可能是玩家id，也可能是自定义参数
--  如果是玩家id，网关底层会自动转发。如果是自定义参数，如连接id，等级要求...
-- 回调clt_multicast_new时会把args_list传回脚本，需要脚本定义处理方式
function SrvMgr:clt_multicast(mask, args_list, cmd, pkt, ecode)
    local srv_conn = self:get_gateway_conn()
    return network_mgr:ssc_multicast(srv_conn.conn_id, mask, args_list,
                                     network_mgr.CDC_PROTOBUF, cmd.i,
                                     ecode or 0, pkt)
end

-- 客户端广播(直接发给客户端，仅网关可用)
-- @conn_list: 客户端conn_id列表
function SrvMgr:raw_clt_multicast(conn_list, cmd, pkt, ecode)
    return network_mgr:clt_multicast(conn_list, network_mgr.CDC_PROTOBUF, cmd.i,
                                     ecode or 0, pkt)
end

-- 底层accept回调
function SrvMgr:srv_conn_accept(conn_id, conn)
    self.srv_conn[conn_id] = conn

    PRINTF("accept server connection:%d", conn_id)

    conn:send_register()
end


-- 服务器之间连接成功
function SrvMgr:srv_conn_new(conn_id, ecode)
    local conn = self.srv_conn[conn_id]
    if 0 ~= ecode then
        PRINTF("connect(%d) to %s:%d error:%s", conn_id, conn.ip, conn.port,
               util.what_error(ecode))
        self.srv_conn[conn_id] = nil

        if conn.auto_conn then self.srv_waiting[conn] = 1 end
        return
    end

    PRINTF("connect(%d) to %s:%d establish", conn_id, conn.ip, conn.port)

    conn:send_register()
end

-- 底层连接断开回调
function SrvMgr:srv_conn_del(conn_id)
    local conn = self.srv_conn[conn_id]

    if conn.session then self.srv[conn.session] = nil end
    self.srv_conn[conn_id] = nil

    PRINTF("%s connect del", conn:conn_name())

    -- TODO: 是否有必要检查对方是否主动关闭
    if conn.auto_conn then self.srv_waiting[conn] = 1 end
end

local srv_mgr = SrvMgr()
return srv_mgr
