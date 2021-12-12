-- 登录、退出
local Loginout = {}

local util = require "engine.util"
local AST = require "modules.ai.ai_header"
g_setting = require "setting.setting" -- require_no_update

-- 检查是否执行登录
function Loginout.check_and_login(ai)
    if (ai.login_time or 0) > ev:time() then return false end

    local conf = g_setting.gateway[1]
    local host = conf.cip
    if "0.0.0.0" == host then
        host = "127.0.0.1"
    elseif "::" == host then
        host = "::1"
    end

    local conn = ai.entity.conn

    -- 连接服务器
    conn:connect(host, conf.cport)
    conn.on_connected = Loginout.on_connected
    conn.on_disconnected = Loginout.on_disconnected

    ai.state = AST.LOGIN
    return true
end

-- 连接成功
function Loginout.on_connected(conn)
    local entity = conn.entity

    printf("android(%d) connection(%d) establish", entity.index, conn.conn_id)

    Loginout.do_login(entity)
end

-- 断开连接
function Loginout.on_disconnected(conn)
    local entity = conn.entity

    entity.ai.state = AST.OFF
    printf("android(%d) die ", entity.index)
end


-- 执行登录逻辑
function Loginout.do_login(entity)
    -- sid:int;        // 服务器id
    -- time:int;       // 时间戳
    -- plat:int;       // 平台id
    -- sign:string;    // 签名
    -- account:string; // 帐户

    local pkt = {
        sid = 1,
        time = ev:time(),
        plat = 999,
        account = string.format("android_%d", entity.index)
    }
    pkt.sign = util.md5(LOGIN_KEY, pkt.time, pkt.account)

    return entity:send_pkt(PLAYER.LOGIN, pkt)
end


-- 登录返回
function Loginout.on_login(entity, errno, pkt)
    -- no role,create one now
    if 0 == (pkt.pid or 0) then
        local _pkt = {name = string.format("android_%d", entity.index)}
        entity:send_pkt(PLAYER.CREATE, _pkt)

        return
    end

    -- 如果已经存在角色，直接请求进入游戏
    entity.pid = pkt.pid
    entity.name = pkt.name
    Loginout.enter_world(entity)
end

-- 创角返回
function Loginout.on_create_role(entity, errno, pkt)
    if 0 ~= errno then
        printf("android_%d unable to create role", entity.index)
        return
    end

    entity.pid = pkt.pid
    entity.name = pkt.name

    Loginout.enter_world(entity)

    printf("android_%d create role success,pid = %d,name = %s", entity.index,
           entity.pid, entity.name)
end

-- 请求进入游戏
function Loginout.enter_world(entity)
    entity:send_pkt(PLAYER.ENTER, {})
end

-- 确认进入游戏完成
function Loginout.on_enter_world(entity, errno, pkt)
    entity.ai.state = AST.ON

    local param = entity.ai.ai_conf.param

    -- 记录登录时间，一段时间后自动退出
    entity.ai.logout_time = ev:time() + math.random(60, param.logout_time)

    printf("%s enter world success", entity.name)
    -- PE.fire_event( PE_ENTER,entity )
end

-- 初始化场景属性
function Loginout.on_init_property(entity, errno, pkt)
    -- 切换进程时，这里的数据会重新下发

    entity.handle = pkt.handle
end

-- 被顶号
function Loginout.on_login_otherwhere(entity, errno, pkt)
    printf("%s login other where", entity.name)
end

-- ************************************************************************** --

-- 是否执行退出
function Loginout.check_and_logout(ai)
    if ai.logout_time > ev:time() then return false end

    local param = ai.ai_conf.param

    ai.state = AST.OFF
    ai.login_time = ev:time() + math.random(60, param.login_time)

    local entity = ai.entity
    entity.conn:close()

    entity.handle = nil -- 要测试重登录，handle会重置

    printf("%s logout", entity.name)

    return true
end

-- ************************************************************************** --


AndroidMgr.reg(PLAYER.LOGIN, Loginout.on_login)
AndroidMgr.reg(PLAYER.CREATE, Loginout.on_create_role)
AndroidMgr.reg(PLAYER.ENTER, Loginout.on_enter_world)
AndroidMgr.reg(PLAYER.OTHER, Loginout.on_login_otherwhere)
AndroidMgr.reg(ENTITY.PROPERTY, Loginout.on_init_property)

return Loginout
