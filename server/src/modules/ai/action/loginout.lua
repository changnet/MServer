-- 登录、退出
local Loginout = {}

local util = require "engine.util"
local AST = require "modules.ai.ai_header"

-- 检查是否执行登录
function Loginout.check_and_login(ai)
    if (ai.login_time or 0) > Engine.time() then return false end

    -- 有开启多个网关的话，随机选一个
    local gateway = table.choice(g_setting.gateway)

    local host = gateway.host
    if "0.0.0.0" == host then
        host = "127.0.0.1"
    elseif "::" == host then
        host = "::1"
    end

    local entity = ai.entity

    local socket = entity.s

    -- 连接服务器
    if socket then
        socket:close()
    end

    socket = entity.socket_driver()
    entity.s = socket

    socket.entity = ai.entity
    socket.on_message = BotMgr.on_message
    socket.on_connected = Loginout.on_connected
    socket.on_disconnected = Loginout.on_disconnected

    socket:connect(host, gateway.port)

    ai.state = AST.LOGIN
    return true
end

-- 连接成功
function Loginout.on_connected(socket)
    local entity = socket.entity

    printf("bot(%d) connection(%d) establish", entity.id, socket.socket_id)

    -- sid:int;        // 服务器id
    -- time:int;       // 时间戳
    -- pfid:int;       // 平台id
    -- sign:string;    // 签名
    -- account:string; // 帐户

    local pkt = {
        sid = 1,
        time = Engine.time(),
        pfid = 999,
        account = string.format("bot_%d", entity.id)
    }
    pkt.sign = util.sha1(LOGIN_KEY, pkt.time, pkt.account)

    return entity:send_pkt(M.PlayerLogin, pkt)
end

local function do_disconnect(socket)
    local entity = socket.entity

    socket:close()
    entity.s = nil
    entity.ai.state = AST.OFF
end

-- 断开连接
function Loginout.on_disconnected(socket)
    local e, msg = socket:get_error()
    printf("bot(%d) die: (%d)%s", socket.entity.id, e, msg)

    do_disconnect(socket)
end

-- 登录返回
local function s_login(entity, pkt)
    if 0 ~= (pkt.errno or 0) then
        printf("bot(%d) login error: %d", entity.id, pkt.errno)
        do_disconnect(entity.s)
        return
    end

    -- no role,create one now
    if 0 == #(pkt.list or EMPTY) then
        local _pkt = {name = string.format("bot_%d", entity.id)}

        printf("bot_%d send create role", entity.id)
        entity:send_pkt(M.PlayerCreate, _pkt)

        return
    end

    -- 如果已经存在角色，直接请求进入游戏
    local role = pkt.list[1]
    local pid = role.pid
    assert(pid and 0 ~= pid)

    entity.pid = pid
    entity.name = role.name
    Loginout.enter_world(entity)
end

-- 创角返回
local function s_create_role(entity, pkt)
    if 0 ~= (pkt.errno or 0) then
        printf("bot(%d) login error: %d", entity.id, pkt.errno)
        do_disconnect(entity.s)
        return
    end

    assert(0 ~= (pkt.pid or 0))

    entity.pid = pkt.pid
    entity.name = pkt.name

    Loginout.enter_world(entity)

    printf("bot_%d create role success,pid = %d,name = %s", entity.id,
           entity.pid, entity.name)
end

-- 请求进入游戏
function Loginout.enter_world(entity)
    printf("bot_%d enter game,pid = %d", entity.id, entity.pid)
    entity:send_pkt(M.PlayerEnter, {pid = entity.pid})
end

-- 确认进入游戏完成
local function s_enter_world(entity, pkt)
    entity.ai.state = AST.ON

    local param = entity.ai.ai_conf.param

    -- 记录登录时间，一段时间后自动退出
    entity.ai.logout_time = Engine.time() + math.random(60, param.logout_time)

    printf("%s enter world success", entity.name)
    -- Event.semit( EV.ENTER,entity )
end

-- 初始化场景属性
local function s_init_base(entity, pkt)
    -- 切换进程时，这里的数据会重新下发

    entity.handle = pkt.handle
    entity.name = pkt.pp_str[1]
end

-- 被顶号
local function s_kick(entity, pkt)
    printf("%s kick offline", entity.name, pkt.reason)
    -- 随后socket会被服务器断开，不需处理
end

-- ************************************************************************** --

-- 是否执行退出
function Loginout.check_and_logout(ai)
    local time = Engine.time()
    if ai.logout_time > time then return false end

    local param = ai.ai_conf.param

    ai.state = AST.OFF
    ai.login_time = time + math.random(60, param.login_time)

    local entity = ai.entity
    entity.socket:close()

    entity.handle = nil -- 要测试重登录，handle会重置

    printf("%s logout", entity.name)

    return true
end

-- ************************************************************************** --

Event.reg(EV.SCRIPT_LOADED, function()
    BotMgr.reg(M.PlayerLogin, s_login)
    BotMgr.reg(M.PlayerCreate, s_create_role)
    BotMgr.reg(M.PlayerEnter, s_enter_world)
    BotMgr.reg(M.PlayerKick, s_kick)
    BotMgr.reg(M.PlayerBase, s_init_base)
end)

return Loginout
