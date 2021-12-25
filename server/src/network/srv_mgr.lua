-- 管理与服务器之间的连接
SrvMgr = {}

local util = require "engine.util"

local SsConn = require "network.ss_conn"
local this = global_storage("SrvMgr", {
    srv = {}, -- 已认证的服务器连接
    srv_conn = {}, -- 所有连接，包括未认证，正在重连中的连接
    srv_waiting = {}, -- 正在重连中的连接
    srv_ready = {}, -- 已经初始化完成的其他服务器
    last_reconnect = 0, -- 上一次尝试重连时间
})

-- 重新连接到其他服务器
local function reconnect_srv()
    local now = ev:time()
    -- N秒后才尝试一次重连
    if now - this.last_reconnect < 2 then return end

    this.last_reconnect = now
    if table.empty(this.srv_waiting) then return end

    local conn_list = this.srv_waiting

    this.srv_waiting = {}
    for conn in pairs(conn_list) do
        local conn_id = conn:reconnect()

        this.srv_conn[conn_id] = conn
        printf("reconnect to %s:%d", conn.ip, conn.port)
    end
end

-- 检查一个连接超时
local tm_pkt = {response = true}
function SrvMgr.check_one_timeout(srv_conn, check_time)
    if not srv_conn.ok then return end -- 还没连接成功的不检查

    local ts = srv_conn:check(check_time)
    if ts > SRV_ALIVE_TIMES then
        printf("%s server timeout", srv_conn:conn_name())

        SrvMgr.close_srv_conn(srv_conn)
        if srv_conn.auto_conn then this.srv_waiting[srv_conn] = 1 end
    elseif ts > 0 and srv_conn.auth then
        srv_conn:send_pkt(SYS.BEAT, tm_pkt)
    end
end

-- 定时器回调
function SrvMgr.do_timer()
    local check_time = ev:time() - SRV_ALIVE_INTERVAL
    for _, srv_conn in pairs(this.srv_conn) do
        SrvMgr.check_one_timeout(srv_conn, check_time)
    end
end

-- 获取服务器连接
function SrvMgr.get_conn_by_session(session)
    return this.srv[session]
end

-- 获取所有服务器连接
function SrvMgr.get_all_srv_conn()
    return this.srv
end

-- 在非网关进程直接发送数据到客户端
function SrvMgr.send_clt_pkt(pid, cmd, pkt, ecode)
    local srv_conn = this.srv[GSE]
    return srv_conn:send_clt_pkt(pid, cmd, pkt, ecode)
end

-- 发送服务器数据包到网关(现在同一类型只有一个进程才能这样做)
function SrvMgr.send_gateway_pkt(cmd, pkt, ecode)
    local srv_conn = this.srv[GSE]

    return srv_conn:send_pkt(cmd, pkt, ecode)
end

-- 发送服务器数据包到世界服(现在同一类型只有一个进程才能这样做)
function SrvMgr.send_world_pkt(cmd, pkt, ecode)
    local srv_conn = this.srv[WSE]

    return srv_conn:send_pkt(cmd, pkt, ecode)
end

-- 根据conn_id获取连接
function SrvMgr.get_conn(conn_id)
    return this.srv_conn[conn_id]
end

-- 主动关闭服务器链接
function SrvMgr.close_srv_conn(conn)
    conn:close()
    SrvMgr.set_conn(conn.conn_id, nil)

    if conn.session then this.srv[conn.session] = nil end
    this.srv_conn[conn.conn_id] = nil
end

-- 服务器广播
function SrvMgr.srv_multicast(cmd, pkt, ecode)
    local conn_list = {}
    for _, conn in pairs(this.srv) do table.insert(conn_list, conn.conn_id) end
    return network_mgr:srv_multicast(conn_list, network_mgr.CDT_PROTOBUF, cmd.i,
                                     ecode or 0, pkt)
end

-- 客户端广播
-- 非网关向客户端广播，由网关转发.网关必须处理clt_multicast_new回调
-- @mask:掩码，参考C++宏定义clt_multicast_t。
-- @args_list:参数列表，根据掩码，这个参数可能是玩家id，也可能是自定义参数
--  如果是玩家id，网关底层会自动转发。如果是自定义参数，如连接id，等级要求...
-- 回调clt_multicast_new时会把args_list传回脚本，需要脚本定义处理方式
function SrvMgr.clt_multicast(mask, args_list, cmd, pkt, ecode)
    local srv_conn = this.srv[GSE]
    return network_mgr:ssc_multicast(srv_conn.conn_id, mask, args_list,
                                     network_mgr.CDT_PROTOBUF, cmd.i,
                                     ecode or 0, pkt)
end

-- 客户端广播(直接发给客户端，仅网关可用)
-- @conn_list: 客户端conn_id列表
function SrvMgr.raw_clt_multicast(conn_list, cmd, pkt, ecode)
    return network_mgr:clt_multicast(conn_list, network_mgr.CDT_PROTOBUF, cmd.i,
                                     ecode or 0, pkt)
end

-- 底层accept回调
function SrvMgr.srv_conn_accept(conn)
    local conn_id = conn.conn_id
    this.srv_conn[conn_id] = conn

    printf("accept server connection:%d", conn_id)
end

function SrvMgr.on_conn_ok(conn_id)
    -- 之所以不直接传conn对象进来是为了在这里检测该conn_id已在srvmgr中记录
    local conn = this.srv_conn[conn_id]

    -- 发送认证数据
    local pkt = {
        session = g_app.session,
        timestamp = ev:time()
    }
    pkt.auth = util.md5(SRV_KEY, pkt.timestamp, pkt.session)
    conn:send_pkt(SYS.REG, pkt)
end

-- 底层连接断开回调
function SrvMgr.srv_conn_del(conn_id, e, is_conn)
    -- 这个函数会触发两次，一次是连接失败，一次是socket关闭
    -- 连接失败
    local conn = this.srv_conn[conn_id]
    if is_conn then
        printf("connect to %s FAIL: %s", conn:base_name(), util.what_error(e))
        return
    end

    if conn.session then this.srv[conn.session] = nil end
    this.srv_conn[conn_id] = nil

    printf("%s connect del", conn:conn_name())

    -- TODO: 是否有必要检查对方是否主动关闭
    if conn.auto_conn then this.srv_waiting[conn] = 1 end

    SE.fire_event(SE_SRV_DISCONNTED, conn)
end

-- 其他服务器是否初始化完成
function SrvMgr.is_other_srv_ready(name, index)
    if not this.srv_ready[name] then return false end

    return this.srv_ready[name][index]
end

-- 其他服务器初始化完成
function SrvMgr.on_other_srv_ready(name, index, id, session)
    if not this.srv_ready[name] then this.srv_ready[name] = {} end

    this.srv_ready[name][index] = true
    print("other srv ready", name, index, id, session)
end

-- 当前服务器初始化完成
local function on_srv_ready(name, index, id, session)
    for _, conn in pairs(this.srv) do
        Rpc.conn_call(conn, SrvMgr.on_other_srv_ready, name, index, id, session)
    end
end

local function on_app_start(check)
    -- 监听其他服务器连接
    if not check then
        local ip = g_app_setting.sip
        local port = g_app_setting.sport
        if ip then
             this.srv_listen_conn = SsConn()
             local ok, msg = this.srv_listen_conn:listen(ip, port)
             if ok then
                printf("listen for server at %s:%d", ip, port)
             else
                printf("listen for server at %s:%d error: %s", ip, port, msg)
             end
        end
    else
        -- 上一次没监听成功
        if g_app_setting.sip and not this.srv_listen_conn.ok then
            return false
        end
    end

    -- 有些app不需要主动连到其他服务器，如gateway
    local srvs = g_app_setting.servers or {}

    -- 检测连接是否完成
    -- 这里只检测连接上，不检测数据同步
    -- 如果某些模块需要数据同步完成才能起服，该模块需要自己注册一个app_start事件
    if check then
        -- 有些服务器起得慢来不及监听，要不断地去尝试重连
        reconnect_srv()
        return table.size(this.srv) == #srvs
    end

    -- 主动连接到其他服务器
    if 0 == #srvs then return true end

    for _, name in pairs(srvs) do
        -- 对方可能有多个进程，如多个网关
        for index, setting in pairs(g_setting[name]) do
            local conn = SsConn()

            conn.auto_conn = true
            local conn_id = conn:connect(setting.sip, setting.sport)

            this.srv_conn[conn_id] = conn
            this.srv_waiting[conn] = 1
            printf("connect to app = %s index = %d at %s:%d",
                name, index, setting.sip, setting.sport)
        end
    end

    return false
end

-- 服务器认证
local function handle_srv_reg(conn, pkt)
    local session = pkt.session
    local auth = util.md5(SRV_KEY, pkt.timestamp, session)
    if pkt.auth ~= auth then
        elog("SrvMgr.srv_register fail,session %d", session)
        return false
    end

    -- TODO 这个可能会有点问题
    -- 如果以后有连跨服，他们是否会分配不同的id，是的话要改下
    local _, _, id = App.decode_session(session)
    if id ~= tonumber(g_app.id) then
        elog("SrvMgr.srv_register id not match,expect %s,got %d", g_app.id,
              id)
        return
    end

    if this.srv[session] then
        elog("SrvMgr.srv_register session conflict:%d", session)
        return false
    end

    this.srv[session] = conn
    network_mgr:set_conn_session(conn.conn_id, session)

    conn:authorized(pkt)
    printf("%s register succes:session %d", conn:conn_name(), session)

    SE.fire_event(SE_SRV_CONNTED, conn)

    return true
end


-- 启动优先级略高于普通模块，普通模块可能需要连接来从其他服同步数据
App.reg_start("SrvMgr", on_app_start, 15)

SE.reg(SE_READY, on_srv_ready)
Cmd.reg_srv(SYS.REG, handle_srv_reg, nil, true)

return SrvMgr
