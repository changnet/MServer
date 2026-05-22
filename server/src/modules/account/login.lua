-- 登录逻辑处理
Login = {}

-- 顶号处理
function Login.login_else_where(session_id, account, pfid)
    local socket = CltMgr.get_by_session_id(session_id)
    -- 用户掉线
    if not socket then
        printf("login else where socket not found, maybe offline account=%s",
            account)
        return
    end

    NetMsg.send_socket(socket, M.PlayerKick, {reason = 1})
    print("login else where, socket close", account, socket.pid)

    socket:close(true)
end

function Login.do_login_result(session_id, account, pfid, info, e)
    local socket = CltMgr.get_by_session_id(session_id)
    -- 用户掉线
    if not socket then
        print("login result socket not found", account, pfid)
        return
    end
    assert(socket.login.account == account)

    socket.role = info

    -- 返回角色信息(如果没有角色，则pid和name都为nil)
    NetMsg.send_socket(socket, M.PlayerLogin, info)

    printf("client login success, account=%s, pfid=%d", account, pfid)
end

-- 玩家登录
local function c_player_login(socket, pkt)
    local time = pkt.time or 0
    local account = pkt.account or ""
    -- 帐号account在不同平台会重复，因此pfid才能确定一个玩家
    -- 可能会合服，因此sid也需要
    local pfid = pkt.pfid or 0
    local sid = pkt.sid or -1

    local session_id = assert(socket.session_id)

    printf("client login socketid = %d, acc = %s, pfid=%d, sid=%d",
        session_id, account, pfid, sid)

    if Engine.time() - time > 1800 then
        eprint("player login time expire", account, time)
        return NetMsg.send_socket(socket, M.PlayerLogin, {errno = E.SIGN_EXPIRE})
    end

    -- sha1可以直接传数字，会自动转成string。但有可能会带.0导致出错
    local sign = Util.sha1(LOGIN_KEY, math.tointeger(time), account)
    if sign ~= pkt.sign then
        eprint("clt sign error:", time, account, pkt.sign, sign)
        return NetMsg.send_socket(socket, M.PlayerLogin, {errno = E.PWD_ERROR})
    end

    -- 不能重复发送(不是顶号，顶号socket_id不应该会重复)
    if socket.login then
        eprint("player login already in process", account)
        return NetMsg.send_socket(socket, M.PlayerLogin, {errno = E.UNDEFINE})
    end
    socket.login = pkt
    socket.account = account -- 复制一份，日志用

    -- 网关有多个，需要统一到帐号管理那边获取角色数据，处理顶号
    local addr = Router.find_worker_addr(W.ACCOUNT, "account", account)
    Send[addr].AccountMgr.login(LOCAL_ADDR, session_id, account, pfid, sid)
end

-- 返回创角结果
-- @param info table 账号信息
-- @param pid integer 创角成功的角色id
-- @param e integer 错误码
function Login.do_create_result(session_id, info, pid, e)
    local account = info.account
    local socket = CltMgr.get_by_session_id(session_id)
    -- 用户掉线
    if not socket then
        print("login create socket not found", account)
        return
    end

    assert(socket.login.account == account)

    socket.role = info
    local role = table.find_value(info.list, "pid", pid)

    -- 返回角色信息(如果没有角色，则pid和name都为nil)
    NetMsg.send_socket(socket, M.PlayerCreate, {
        pid = pid,
        name = role and role.property.name,
        errno = e
    })

    printf("client create role success, acc = %s, pid = %d", account, pid)
end

-- 创角
local function c_create_role(socket, pkt)
    local role_info = socket.role
    local login_info = socket.login

    local session_id = socket.session_id
    if not role_info or not login_info then
        eprint("create role no account info", session_id)
        return
    end

    local account = login_info.account

    -- 当前一个帐号只能创建一个角色
    if #(role_info.list or EMPTY) > 0 then
        eprint("role already create", session_id, account)
        return
    end

    -- TODO: 检测一个名字是否带有数据库非法字符和敏感字,是否重复

    local addr = Router.find_worker_addr(W.ACCOUNT, "account", account)
    Send[addr].AccountMgr.create_role(
        session_id, account, login_info.pfid, login_info.sid, pkt)
end

-- 角色进入游戏
local function c_enter_game(socket, pkt)
    local role_info = socket.role
    local login_info = socket.login

    local session_id = socket.session_id
    if not role_info or not login_info then
        eprint("enter game no account info", session_id)
        return
    end

    local account = login_info.account
    if socket.pid then
        eprint("role already enter game", session_id, account, socket.pid)
        return
    end

    -- 角色不存在
    local role
    local pid = pkt.pid
    for _, info in pairs(role_info.list or EMPTY) do
        if pid and info.pid == pid then
            role = info
            break
        end
    end
    if not role then
        eprint("role not exist", session_id, account, pid)
        return
    end

    local ip = socket:address()

    CltMgr.bind(socket, pid)
    local addr = Router.find_worker_addr(W.ACCOUNT, "account", account)
    Send[addr].AccountMgr.enter(
        session_id, account, login_info.pfid, login_info.sid, pid, ip)
end

-- enter_game完成
function Login.enter_game_completed(pid, session_id, paddr, route_info)
    --[[
    这里有一点问题：
    1. player线程登录
    2. 线程1初始化并发送协议1
    3. player线程登录成功，通知前端enter_game成功

    虽然player线程保证线程1已经发出协议后再执行登录成功逻辑
    但线程1可能是一个远程节点，它发送的协议1还没发到网关，没有转发到客户端时
    而player线程更快，已经通知网关把enter_game发给客户端，这就导致客户端收到
    enter_game成功时，并没有收到所有数据

    解决的办法：
        1. 服务端弄一套机制，所有必要的线程初始化完都通知网关，网关负责记录，直到所有线程
    完成后再发登录成功给客户端。
        2. 客户端只把登录成功当成可以下发协议的开关，不保证数据完全到达
        3. 特殊处理：某个协议前端一定要拿到的，后端用call解决，一般也不多
        4. 如果是单进程部署，根本就不存在这个问题
    ]]

    local socket = CltMgr.get_by_pid(pid)
    if not socket then
        print("enter_game_completed player disconnect", pid, session_id)
        return
    end
    if socket.session_id ~= session_id then
        print("enter_game_completed session no match", pid, session_id)
        return
    end

    socket:authorized()
    Router.update_player_comm_addr(pid, paddr, LOCAL_ADDR)

    -- 设置其他路由信息，比如场景线程等
    for wtype, addr in pairs(route_info or EMPTY) do
        Router.update_player_addr(pid, wtype, addr)
    end

    return NetMsg.send_socket(socket, M.PlayerEnter, EMPTY)
end

NetMsg.reg_noauth(M.PlayerLogin, c_player_login)
NetMsg.reg_noauth(M.PlayerCreate, c_create_role)
NetMsg.reg_noauth(M.PlayerEnter, c_enter_game)
