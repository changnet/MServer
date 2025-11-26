-- 登录逻辑处理
Login = {}

-- 顶号处理
function Login.login_else_where(socket_id, account, pfid)
    local socket = CltMgr.get(socket_id)
    -- 用户掉线
    if not socket then
        print("login socket not found", account, pfid)
        return
    end

    socket:send_pkt(PLAYER.KICK, {reason = 1})
    print("login else where", account, socket.pid)

    socket:close(true)
end

function Login.do_login_result(socket_id, account, pfid, info, e)
    local socket = CltMgr.get(socket_id)
    -- 用户掉线
    if not socket then
        print("login socket not found", account, pfid)
        return
    end
    assert(socket.login.account == account)

    socket.role = info

    -- 返回角色信息(如果没有角色，则pid和name都为nil)
    socket:send_pkt(PLAYER.LOGIN, info)

    printf("client login success:%s--%d", account, pfid)
end

-- 玩家登录
local function c_player_login(socket, pkt)
    local time = pkt.time or 0
    local account = pkt.account or ""
    -- 帐号account在不同平台会重复，因此pfid才能确定一个玩家
    -- 可能会合服，因此sid也需要
    local pfid = pkt.pfid or 0
    local sid = pkt.sid or -1

    local socket_id = socket.socket_id

    printf("client login socketid = %d, acc = %s, pfid=%d, sid=%d",
        socket_id, account, pfid, sid)

    if Engine.time() - time > 1800 then
        eprint("player login time expire", account, time)
        return
    end

    local sign = Util.md5(LOGIN_KEY, time, account)
    if sign ~= pkt.sign then
        eprint("clt sign error:", time, account, pkt.sign, sign)
        return
    end

    -- 不能重复发送(不是顶号，顶号socket_id不应该会重复)
    if socket.login then
        eprint("player login already in process", account)
        return
    end
    socket.login = pkt

    -- 网关有多个，需要统一到帐号管理那边获取角色数据，处理顶号
    local addr = Router.find_worker_addr(W_ACCOUNT, account)
    Send.AccountMgr.login(addr, LOCAL_ADDR, socket_id, account, pfid, sid)
end

function Login.do_create_result(socket_id, account, pfid, info, e)
    local socket = CltMgr.get(socket_id)
    -- 用户掉线
    if not socket then
        print("login socket not found", account, pfid)
        return
    end
    assert(socket.login.account == account)

    socket.role = info
    local role = assert(info.list[1])

    -- 返回角色信息(如果没有角色，则pid和name都为nil)
    socket:send_pkt(PLAYER.CREATE, role)

    printf("client create role success, acc = %s, pid = %d", account, role.pid)
end

-- 创角
local function c_create_role(socket, pkt)
    local role_info = socket.role
    local login_info = socket.login

    local socket_id = socket.socket_id
    if not role_info or not login_info then
        eprint("create role no account info", socket_id)
        return
    end

    local account = login_info.account

    -- 当前一个帐号只能创建一个角色
    if #(role_info.list or EMPTY) > 0 then
        eprint("role already create", socket_id, account)
        return
    end

    -- TODO: 检测一个名字是否带有数据库非法字符和敏感字,是否重复

    local addr = Router.find_worker_addr(W_ACCOUNT, account)
    Send.AccountMgr.create_role(addr, LOCAL_ADDR,
        socket_id, account, login_info.pfid, login_info.sid, pkt)
end

-- 角色进入游戏
local function c_enter_game(socket, pkt)
    local role_info = socket.role
    local login_info = socket.login

    local socket_id = socket.socket_id
    if not role_info or not login_info then
        eprint("enter game no account info", socket_id)
        return
    end

    local account = login_info.account
    if socket.pid then
        eprint("role already enter game", socket_id, account, socket.pid)
        return
    end

    -- 角色不存在
    local role
    local pid = pkt.pid
    for _, info in pairs(role_info.list or EMPTY) do
        if info.pid == pid then
            role = info
            break
        end
    end
    if not role then
        eprint("role not exist", socket_id, account, pid)
        return
    end

    socket.pid = pid
    local addr = Router.find_worker_addr(W_ACCOUNT, account)
    Send.AccountMgr.enter(addr, LOCAL_ADDR,
        socket_id, account, login_info.pfid, login_info.sid, pid)
end

NetMsg.reg_noauth(PLAYER.LOGIN, c_player_login)
NetMsg.reg_noauth(PLAYER.CREATE, c_create_role)
NetMsg.reg_noauth(PLAYER.ENTER, c_enter_game)
