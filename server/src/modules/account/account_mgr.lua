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
    多个网关，每个网关都负责通信和帐号管理
    每个网关都负责帐号管理会有个数据竞争的问题要处理，需要每次登录都向所有
    网关同步登录，把在其他网关的设备下线，还要保证在不同网关创建的角色，都有同步到所有网关，
    这个需要用Gossip的协议来做，就很麻烦。
方案3
    多个网关负责通信，多个帐号管理负责帐号管理，可以在承载不高的情况下只开一个帐号管理

开多个帐号管理时，路由策略应该使用redis cluster的一致性hash策略，即网关只根据帐号就能
计算出该帐号在哪个帐号管理负责，直接和该帐号管理通信，而不需要考虑多个帐号管理数据竞争问题

方案2中，理论上也可以使用hash来控制玩家数据在哪个网关处理，但问题是网关是对外的，无法
控制玩家在哪个网关登录，只能在帐号管理这一层来做
]]

local this = memory("AccountMgr", {
    account = {}, -- [pfid][account] = {list = {}} 帐号数据数据缓存
    role_acc = {}, -- [pid] = role_info 当前登录的角色数据
})

Rpc.proxy_wtype("Mongodb", W_MONGODB)

-- 读取帐号数据时，哪些字段要读取
local filter = '{"projection":{"pid":1,"name":1,"sid":1}}'

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

local function return_login_result(info, account, pfid, e)
    Send.Login.do_result(info.addr, account, pfid, info, e)
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

local function get_account_info(account, pfid, sid)
    local pfid_list = this.account[account]
    if not pfid_list then
        pfid_list = {}
        this.account[account] = pfid_list
    end

    local server_list =  pfid_list[pfid]
    if not server_list then
        server_list = {}
        pfid_list[pfid] = server_list
    end
    local info = server_list[sid]
    if not info then
        info = {
            role = {}, -- 角色列表
        }
        server_list[sid] = info
    end
    return info
end

function AccountMgr.login(addr, socket_id, account, pfid, sid)
    -- 传nil会导致下面的数据库查询返回其他数据
    assert(account and pfid and sid)

    local info = get_account_info(account, pfid, sid)
    local old_addr = info.addr
    if old_addr then
        local old_id = info.socket_id
        assert(old_id and old_id ~= socket_id)

        -- 顶号，通知旧的下线
        Send.Login.login_else_where(old_addr, old_id, account, pfid)
    end

    info.addr = addr
    info.socket_id = socket_id

    if info.loaded then
        return return_login_result(info, account, pfid, 0)
    end

    local db_addr = Router.find_worker_addr(W_MONGODB, "role", account)
    info.loaded = 1
    local e, rows = Call.MongoDB.find(db_addr,
        "role", {account = account, pfid = pfid, sid = sid}, filter)
    if 0 ~= e then
        for _, row in pairs(rows) do
            table.insert(info.role, row)
        end
        return_login_result(info, account, pfid, e)
    end
end

return AccountMgr
