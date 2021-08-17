-- account_mgr.lua
-- 2017-04-02
-- xzc

-- 帐号管理
AccMgr = {}

local this = global_storage("AccMgr", {
    account = {}, -- 三级key [sid][plat][account]
    conn_acc = {}, -- conn_id为key，帐号信息为value
    role_acc = {}, -- 玩家pid为key
})

local util = require "util"

-- 根据Pid获取角色数据
function AccMgr.get_role_info(pid)
    return this.role_acc[pid]
end

--  创建角色逻辑
function AccMgr.do_create_role(role_info, pkt, pid)
    local base = {}
    base.new = 1 -- 标识为新创建，未初始化用户
    base._id = pid
    base.tm = ev:time()
    base.sid = role_info.sid
    base.plat = role_info.plat
    base.name = pkt.name
    base.account = role_info.account

    local callback = function(...)
        AccMgr.on_role_create(base, role_info, ...)
    end
    g_mongodb:insert("base", base, callback)
end

-- 创建角色数据库返回
-- 角色base库是由world维护的，这里创建新角色比较重要，需要入库确认.再由world加载
function AccMgr.on_role_create(base, role_info, ecode, res)
    if 0 ~= ecode then
        elog("role create error:name = %s,account = %s,srv = %d,plat = %d",
              base.name, role_info.account, role_info.sid, role_info.plat)
        AccMgr.send_role_create(role_info, E.UNDEFINE)
        return
    end

    -- 将角色与帐号绑定
    return AccMgr.do_acc_create(role_info, base.name, base._id)
end

--  创建帐号
function AccMgr.do_acc_create(role_info, name, pid)
    local acc_info = {}
    acc_info._id = pid
    acc_info.tm = ev:time()
    acc_info.sid = role_info.sid
    acc_info.plat = role_info.plat
    acc_info.name = name
    acc_info.account = role_info.account

    local callback = function(...)
        AccMgr.on_acc_create(acc_info, role_info, ...)
    end
    g_mongodb:insert("account", acc_info, callback)
end

-- 创建角色数据库返回
function AccMgr.on_acc_create(acc_info, role_info, ecode, res)
    if 0 ~= ecode then -- 失败
        AccMgr.send_role_create(role_info, E.UNDEFINE)
        printf("create role error:%s", acc_info.account)
        return
    end

    -- 完善玩家数据
    local pid = acc_info._id
    role_info.pid = pid
    role_info.name = acc_info.name
    printf("create role success:%s--%d", role_info.account, pid)

    this.role_acc[pid] = role_info

    -- 玩家可能断线了，这个clt_conn就不存在了
    local clt_conn = CltMgr.get_conn(role_info.conn_id)
    if clt_conn then
        CltMgr.bind_role(pid, clt_conn)
        AccMgr.send_role_create(role_info)
    end
end

-- 发送角色创建结果
function AccMgr.send_role_create(role_info, ecode)
    -- 玩家可能断线了，这个clt_conn就不存在了
    local clt_conn = CltMgr.get_conn(role_info.conn_id)
    if not clt_conn then return end

    clt_conn:send_pkt(PLAYER.CREATE, role_info, ecode)
end

-- 玩家下线
function AccMgr.role_offline(conn_id)
    local role_info = this.conn_acc[conn_id]
    if not role_info then return end -- 连接上来未登录就断线

    role_info.conn_id = nil
    this.conn_acc[conn_id] = nil
end

-- 根据pid下线
function AccMgr.role_offline_by_pid(pid)
    print("AccMgr.role_offline_by_pid", pid)

    CltMgr.clt_close_by_pid(pid)

    local role_info = this.role_acc[pid]
    if not role_info then
        elog("role_offline_by_pid no role_info found:%d", pid)
        return
    end

    AccMgr.role_offline(role_info.conn_id)
end

-- 帐号在其他地方登录
function AccMgr.login_otherwhere(role_info)
    -- 告诉原连接被顶号
    local old_conn = CltMgr.get_conn(role_info.conn_id)
    old_conn:send_pkt(PLAYER.OTHER, {})

    -- 通知其他服务器玩家下线
    if role_info.pid then
        local pkt = {pid = role_info.pid}
        CltMgr.send_world_pkt(SYS.PLAYER_OTHERWHERE, pkt)
    end

    -- 关闭旧客户端连接
    CltMgr.clt_close(old_conn)
end

-- 加载帐号数据
function AccMgr.db_load()
    g_mongodb:find("account", nil, nil, AccMgr.on_db_loaded)
end

-- db数据加载
function AccMgr.on_db_loaded(ecode, res)
    if 0 ~= ecode then
        elog("account db load error")
        return
    end

    for _, role_info in pairs(res) do
        local sid = role_info.sid
        local plat = role_info.plat
        local account = role_info.account

        if not this.account[sid] then this.account[sid] = {} end
        if not this.account[sid][plat] then this.account[sid][plat] = {} end

        role_info.pid = role_info._id
        this.role_acc[role_info.pid] = role_info
        this.account[sid][plat][account] = role_info
    end

    g_app:one_initialized("acc_data", 1)
end


-- 玩家登录
local function player_login(pkt)
    local sign = util.md5(LOGIN_KEY, pkt.time, pkt.account)
    if sign ~= pkt.sign then
        elog("clt sign error:%s", pkt.account)
        return
    end

    if not PLATFORM[pkt.plat] then
        elog("clt platform error:%s", tostring(pkt.plat))
        return
    end

    local clt_conn = Cmd.last_conn()
    local conn_id = clt_conn.conn_id
    -- 不能重复发送(不是顶号，conn_id不应该会重复)
    if this.conn_acc[conn_id] then
        elog("player login pkt dumplicate send")
        return
    end

    -- 考虑到合服，服务器sid、平台plat、帐号account才能确定一个玩家
    local sid = pkt.sid
    local plat = pkt.plat
    local account = pkt.account

    if not this.account[sid] then this.account[sid] = {} end
    if not this.account[sid][plat] then this.account[sid][plat] = {} end

    local role_info = this.account[sid][plat][account]
    if not role_info then
        role_info = {}
        role_info.sid = sid
        role_info.plat = plat
        role_info.account = account

        this.account[sid][plat][account] = role_info
    end

    -- 当前一个帐号只能登录一个角色,处理顶号
    if role_info.conn_id then
        AccMgr.login_otherwhere(role_info)

        -- 下面开始替换连接
        -- TODO 是否要等待其他服务器返回顶号处理成功再替换连接，防止新连接收到旧连接
        -- 的数据，看游戏需要
    end

    -- 连接认证成功，将帐号和连接绑定。现在可以发送其他协议了
    role_info.conn_id = conn_id
    this.conn_acc[conn_id] = role_info

    clt_conn:authorized()
    if role_info.pid then CltMgr.bind_role(role_info.pid, clt_conn) end

    -- 返回角色信息(如果没有角色，则pid和name都为nil)
    clt_conn:send_pkt(PLAYER.LOGIN, role_info)

    printf("client authorized success:%s--%d", pkt.account, role_info.pid or 0)
end

-- 创角
local function create_role(pkt)
    local clt_conn = Cmd.last_conn()
    local role_info = this.conn_acc[clt_conn.conn_id]
    if not role_info then
        elog("create role,no account info")
        return
    end

    -- 当前一个帐号只能创建一个角色
    if role_info.pid then
        elog("role already create")
        return
    end

    -- TODO: 检测一个名字是否带有数据库非法字符和敏感字,是否重复

    local callback = function(pid)
        return AccMgr.do_create_role(role_info, pkt, pid)
    end
    return g_unique_id:player_id(role_info.sid, callback)
end

if APP_TYPE == GATEWAY then
    Cmd.reg(PLAYER.LOGIN, player_login, true)
    Cmd.reg(PLAYER.CREATE, create_role, true)
end

return AccMgr
