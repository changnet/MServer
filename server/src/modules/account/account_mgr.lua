-- account_mgr.lua
-- 2017-04-02
-- xzc

-- 帐号管理
AccountMgr = {}

--[[
网关负责和客户端通信和帐号管理，需要支持多网关
方案1
    多个网关负责通信，一个帐号管理负责帐号管理。这个承载也很高了，但存在一个集中式的帐号
    管理总是不太好
方案2
    多个网关，每个网关都负责通信和帐号管理。但要注意多帐号管理时数据安全问题

我更趋向方案2，至于数据安全问题
    1. 创角是直接在数据库操作，是安全的
    2. 角色缓存数据同步问题
        1. 同步放data或者redis中，完全没必要
        2. 放当前worker中，缓存有更新时，广播到所有网关
        3. 放当前worker中，从某个网关登录时，移除其他网关缓存
        考虑到缓存只有名字、等级需要更新，一般开的网关比较少，用方案2
]]

local this = memory("AccountMgr", {
    account = {}, -- [pfid][account] = {list = {}}
    sock_acc = {}, -- [socket_id] = role_info
    role_acc = {}, -- [pid] = role_info
})

-- 根据Pid获取角色数据
function AccountMgr.get_role_info(pid)
    return this.role_acc[pid]
end

--  创建角色逻辑
function AccountMgr.do_create_role(role_info, pkt, pid)
    local base = {}
    base.new = 1 -- 标识为新创建，未初始化用户
    base._id = pid
    base.tm = ev:time()
    base.sid = role_info.sid
    base.pfid = role_info.pfid
    base.name = pkt.name
    base.account = role_info.account

    local callback = function(...)
        AccountMgr.on_role_create(base, role_info, ...)
    end
    g_mongodb:insert("base", base, callback)
end

-- 创建角色数据库返回
-- 角色base库是由world维护的，这里创建新角色比较重要，需要入库确认.再由world加载
function AccountMgr.on_role_create(base, role_info, ecode, res)
    if 0 ~= ecode then
        eprint("role create error:name = %s,account = %s,srv = %d,pfid = %d",
              base.name, role_info.account, role_info.sid, role_info.pfid)
        AccountMgr.send_role_create(role_info, E.UNDEFINE)
        return
    end

    -- 将角色与帐号绑定
    return AccountMgr.do_acc_create(role_info, base.name, base._id)
end

--  创建帐号
function AccountMgr.do_acc_create(role_info, name, pid)
    local acc_info = {}
    acc_info._id = pid
    acc_info.tm = ev:time()
    acc_info.sid = role_info.sid
    acc_info.pfid = role_info.pfid
    acc_info.name = name
    acc_info.account = role_info.account

    local callback = function(...)
        AccountMgr.on_acc_create(acc_info, role_info, ...)
    end
    g_mongodb:insert("account", acc_info, callback)
end

-- 创建角色数据库返回
function AccountMgr.on_acc_create(acc_info, role_info, ecode, res)
    if 0 ~= ecode then -- 失败
        AccountMgr.send_role_create(role_info, E.UNDEFINE)
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
    local clt_conn = CltMgr.get_conn(role_info.socket_id)
    if clt_conn then
        CltMgr.bind_role(pid, clt_conn)
        AccountMgr.send_role_create(role_info)
    end
end

-- 发送角色创建结果
function AccountMgr.send_role_create(role_info, ecode)
    -- 玩家可能断线了，这个clt_conn就不存在了
    local clt_conn = CltMgr.get_conn(role_info.socket_id)
    if not clt_conn then return end

    clt_conn:send_pkt(PLAYER.CREATE, role_info, ecode)
end

-- 玩家下线
function AccountMgr.role_offline(socket_id)
    local role_info = this.sock_acc[socket_id]
    if not role_info then return end -- 连接上来未登录就断线

    role_info.socket_id = nil
    this.sock_acc[socket_id] = nil
end

-- 根据pid下线
function AccountMgr.role_offline_by_pid(pid)
    print("AccountMgr.role_offline_by_pid", pid)

    CltMgr.close_by_pid(pid)

    local role_info = this.role_acc[pid]
    if not role_info then
        eprint("role_offline_by_pid no role_info found:%d", pid)
        return
    end

    AccountMgr.role_offline(role_info.socket_id)
end

-- 帐号在其他地方登录
function AccountMgr.login_otherwhere(role_info)
    -- 告诉原连接被顶号
    local old_conn = CltMgr.get_conn(role_info.socket_id)
    old_conn:send_pkt(PLAYER.OTHER, {})

    -- 通知其他服务器玩家下线
    if role_info.pid then
        local pkt = {pid = role_info.pid}
        CltMgr.send_world_pkt(SYS.PLAYER_OTHERWHERE, pkt)
    end

    -- 关闭旧客户端连接
    CltMgr.close(old_conn)
end

-- db数据加载
function AccountMgr.on_db_loaded(ecode, res)
    if 0 ~= ecode then
        eprint("account db load error")
        return
    end

    for _, role_info in pairs(res) do
        local sid = role_info.sid
        local pfid = role_info.pfid
        local account = role_info.account

        if not this.account[sid] then this.account[sid] = {} end
        if not this.account[sid][pfid] then this.account[sid][pfid] = {} end

        role_info.pid = role_info._id
        this.role_acc[role_info.pid] = role_info
        this.account[sid][pfid][account] = role_info
    end

    this.ok = true
end

local function get_account_info(account, pfid)
    local pfid_list = this.account[account]
    if not pfid_list then
        pfid_list = {}
        this.account[account] = pfid_list
    end

    local info =  pfid_list[pfid]
    if not info then
        info = {}
        pfid_list[pfid] = info
    end
    return info
end

-- 玩家登录
local function player_login(socket, pkt)
    local time = pkt.time
    local account = pkt.account

    if Engine.time() - time > 1800 then
        eprint("player login time expire", account)
        return
    end

    local sign = Util.md5(LOGIN_KEY, time, account)
    if sign ~= pkt.sign then
        eprint("clt sign error:", time, account, pkt.sign, sign)
        return
    end

    local socket_id = socket.socket_id
    -- 不能重复发送(不是顶号，顶号socket_id不应该会重复)
    if this.sock_acc[socket_id] then
        eprint("player login already in process", account)
        return
    end

    -- 帐号account在不同平台会重复，还需要平台pfid才能确定一个玩家
    local pfid = pkt.pfid

    local role_info = get_account_info(account, pfid)

    -- 当前一个帐号只能登录一个角色,处理顶号
    if role_info.socket_id then
        AccountMgr.login_otherwhere(role_info)

        -- 下面开始替换连接
        -- TODO 是否要等待其他服务器返回顶号处理成功再替换连接，防止新连接收到旧连接
        -- 的数据，看游戏需要
    end

    -- 连接认证成功，将帐号和连接绑定。现在可以发送其他协议了
    role_info.socket_id = socket_id
    this.sock_acc[socket_id] = role_info

    socket:authorized()

    -- 返回角色信息(如果没有角色，则pid和name都为nil)
    socket:send_pkt(PLAYER.LOGIN, role_info)

    printf("client login success:%s--%d", pkt.account, role_info.pid or 0)
end

-- 创角
local function create_role(pkt)
    local clt_conn = Cmd.last_conn()
    local role_info = this.sock_acc[clt_conn.socket_id]
    if not role_info then
        eprint("create role,no account info")
        return
    end

    -- 当前一个帐号只能创建一个角色
    if role_info.pid then
        eprint("role already create")
        return
    end

    -- TODO: 检测一个名字是否带有数据库非法字符和敏感字,是否重复

    local callback = function(pid)
        return AccountMgr.do_create_role(role_info, pkt, pid)
    end
    return g_unique_id:player_id(role_info.sid, callback)
end

NetMsg.reg_noauth(PLAYER.LOGIN, player_login)
NetMsg.reg_noauth(PLAYER.CREATE, create_role)

return AccountMgr
