-- 登录逻辑处理
Login = {}

-- 顶号处理
function Login.login_else_where(socket_id, account, pfid)
end

function Login.do_result(account, pfid, info, e)
    socket:authorized()

    -- 返回角色信息(如果没有角色，则pid和name都为nil)
    socket:send_pkt(PLAYER.LOGIN, info)

    printf("client login success:%s--%d", account, pfid)
end

-- 玩家登录
local function c_player_login(socket, pkt)
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

    -- 不能重复发送(不是顶号，顶号socket_id不应该会重复)
    if socket.login then
        eprint("player login already in process", account)
        return
    end
    socket.login = pkt

    -- 帐号account在不同平台会重复，因此pfid才能确定一个玩家
    -- 可能会合服，因此sid也需要
    local pfid = pkt.pfid
    local sid = pkt.sid

    -- 网关有多个，需要统一到帐号管理那边获取角色数据，处理顶号
    local addr = Router.find_worker_addr(W_ACCOUNT, account)
    Send.AccountMgr.login(addr, LOCAL_ADDR, socket.socket_id, account, pfid, sid)
end

-- 创角
local function c_create_role(pkt)
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
        return AccountMgr.do_c_create_role(role_info, pkt, pid)
    end
    return g_unique_id:player_id(role_info.sid, callback)
end

NetMsg.reg_noauth(PLAYER.LOGIN, c_player_login)
NetMsg.reg_noauth(PLAYER.CREATE, c_create_role)
