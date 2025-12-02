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
    wait_enter = {}, -- [info] = info 等待进入游戏的玩家
})

-- 读取帐号数据时，哪些字段要读取
local ROLE_FILTER = '{"projection":{"pid":1,"name":1,"sid":1}}'
local ROLE_ID_FILTER = string.format('{"_id":%d}', UNIQUEID.PLAYER)
local ROLE_ID_OPTS = '{"$inc":{"seed":1}}'

-- 根据Pid获取角色数据
function AccountMgr.get_role_info(pid)
    return this.role_acc[pid]
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

local function return_login_result(info, account, pfid, e)
    local socket_id = info.socket_id
    if not socket_id then return end -- 数据加载时，已断开

    Send.Login.do_create_result(info.gaddr, socket_id, account, pfid, info, e)
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
            sid = sid,
            pfid = pfid,
            account = account,
            list = {}, -- 角色列表
        }
        server_list[sid] = info
    end
    return info
end

function AccountMgr.login(addr, socket_id, account, pfid, sid)
    -- 传nil会导致下面的数据库查询返回其他数据
    assert(account and pfid and sid)

    local info = get_account_info(account, pfid, sid)
    local old_addr = info.gaddr
    if old_addr then
        local old_id = info.socket_id
        assert(old_id and old_id ~= socket_id)

        this.wait_enter[info] = nil
        -- 如果下线失败，超时将直接顶掉
        if info.paddr and not info.kick_time then
            local pid = info.pid
            info.kick_time = Engine.time()
            Send.PlayerMgr.kick(info.paddr, pfid)
            print("account login kick else where", account, pfid, sid, pid)
        else
            print("account login else where", account, pfid, sid)
        end

        -- 顶号，通知旧的下线
        Send.Login.login_else_where(old_addr, old_id, account, pfid)
    end

    info.gaddr = addr
    info.socket_id = socket_id

    local loaded = info.loaded
    if 2 == loaded then
        -- 数据已加载完成，直接返回结果
        return return_login_result(info, account, pfid, 0)
    elseif 1 == loaded then
        return -- 加载中，不要重复加载
    end

    -- 如果有多个account_mgr，account_mgr的路由已经保证同一个帐号必定在同一个account_mgr处理
    -- 因此对mongodb的路由就没有要求了
    local db_addr = Router.find_worker_addr(W_MONGODB, "role", account)
    info.loaded = 1
    local e, rows = Call.MongoDB.find(db_addr,
        "role", {account = account, pfid = pfid, sid = sid}, ROLE_FILTER)
    if 0 == e then
        for _, row in pairs(rows) do
            local pid = row._id
            row.pid = pid
            table.insert(info.list, row)
        end
        info.loaded = 2
    else
        eprint("account db load error", e, rows)
        e = E.SRV_ERROR
        info.loaded = nil
    end
    return_login_result(info, account, pfid, e)
end

local function return_create_role_result(info, account, pfid, e)
    local socket_id = info.socket_id
    if not socket_id then return end -- 数据加载时，已断开

    Send.Login.do_result(info.addr, socket_id, account, pfid, info, e)
end

-- 创角
function AccountMgr.create_role(socket_id, account, pfid, sid, pkt)
    assert(account and pfid and sid)

    local info = get_account_info(account, pfid, sid)
    if 2 ~= info.loaded then
        eprint("create role not loading", socket_id, account)
        return
    end
    -- 当前一个帐号只能创建一个角色
    if #(info.list or EMPTY) > 0 then
        eprint("role already create", socket_id, account)
        return
    end

    -- 创角中，不要重复创建
    if 1 == info.created then return end

    info.created = 1
    local db_addr = Router.find_worker_addr(W_MONGODB, "uniqueid")

    -- 直接从数据库获取自增id，保证在多个account_mgr下角色id是唯一的
    -- 如果嫌数据库慢，估计要按拼接id的方式在不同account_mgr生成
    -- 或者弄一个id生成worker来专门处理这个事情
    local e, row = Call.MongoDB.find_and_modify(db_addr,
        "uniqueid", ROLE_ID_FILTER, nil, ROLE_ID_OPTS, nil, false, true, true)

    info.created = nil
    if 0 ~= e then
        eprint("uniqueid db load error", e, row)
        return return_create_role_result(info, account, pfid, E.SRV_ERROR)
    end
    local seed = assert(row.seed)
    local real_sid = Engine.get_server_id()
    -- 如果希望这个值比较小，可以采用protobuf的压缩机制来压缩下
    local pid = (seed << PID_SRV_BIT) | real_sid
    local role = {
        account = account,
        pfid = pfid,
        sid = sid,
        _id = pid,
        name = pkt.name,
        level = 0,
        create_time = Engine.time(),
    }

    info.created = 1
    local msg
    e, msg = Call.MongoDB.insert(db_addr, "role", role)
    info.created = nil
    if 0 == e then
        role._id = pid
        table.insert(info.list, role)
    else
        eprint("role db insert error", e, msg)
        e = E.SRV_ERROR
    end

    return return_create_role_result(info, account, pfid, e)
end

-- 处理进入player worker逻辑
local function do_enter(info)
    -- 如果存在paddr，则进入同一个player worker(这种情况一般是下线失败或者超时才会出现)
    -- 否则按player worker路由分配一个

    local pid = info.pid
    local paddr = info.paddr or Router.find_worker_addr(W_PLAYER, pid)

    -- 这个session_id现在只用于同一个玩家区分不同登录会话，因此不要求唯一
    local session_id = info.socket_id
    if session_id == info.session_id then session_id = session_id + 1 end

    local role_info = {
        pid = pid,
        session_id = session_id,
        ip = info.ip,
    }
    info.paddr = paddr
    info.kick_time = nil
    info.session_id = session_id

    print("role enter", info.account, pid, paddr)
    Send.PlayerMgr.enter(paddr, role_info)
end

-- 进入游戏
function AccountMgr.enter(socket_id, account, pfid, sid, pid, ip)
    local info = get_account_info(account, pfid, sid)
    if 2 ~= info.loaded then
        eprint("enter game not loaded", socket_id, account)
        return
    end
    local role
    for _, v in pairs(info.list) do
        if v.pid == pid then
            role = v
            break
        end
    end
    if not role then
        eprint("enter game role not found", socket_id, account, pid)
        return
    end

    info.pid = pid
    info.ip = ip
    -- 如果存在paddr，则说明现在是顶号，旧的玩家还没完全下线
    local paddr = info.paddr
    if not paddr or Engine.time() - (info.kick_time or 0) > 120 then
        do_enter(info)
    else
        -- 等待下线
        this.wait_enter[info] = true
        print("role wait enter", account, pid)
    end
end

return AccountMgr
