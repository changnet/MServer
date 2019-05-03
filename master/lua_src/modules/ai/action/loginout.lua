-- 登录、退出

local AST = require "modules.ai.ai_header"

local Loginout = oo.class( nil,... )

-- 检查是否执行登录
function Loginout:check_and_login(ai)
    local param = ai.ai_conf.param
    if (ai.logout_time or 0) + param.login_time > ev:time() then
        return false
    end

    -- 连接服务器
    local conn_id =
            network_mgr:connect( "127.0.0.1",10002,network_mgr.CNT_CSCN )

    ai.state = AST.LOGIN
    ai.entity:set_conn(conn_id)
    return true
end

-- 执行登录逻辑
function Loginout:do_login(entity)
    -- sid:int;        // 服务器id
    -- time:int;       // 时间戳
    -- plat:int;       // 平台id
    -- sign:string;    // 签名
    -- account:string; // 帐户

    local pkt =
    {
        sid  = 1,
        time = ev:time(),
        plat = 999,
        account = string.format( "android_%d",entity.index )
    }
    pkt.sign = util.md5( LOGIN_KEY,pkt.time,pkt.account )

    return entity:send_pkt( CS.PLAYER_LOGIN,pkt )
end


-- 断开连接
function Loginout:on_disconnect( entity )
    PRINTF( "android(%d) die ",entity.index )
end

-- 登录返回
function Loginout:on_login( entity,errno,pkt )
    -- no role,create one now
    if 0 == (pkt.pid or 0) then
        local _pkt = { name = string.format( "android_%d",entity.index ) }
        entity:send_pkt( CS.PLAYER_CREATE,_pkt )

        return
    end

    -- 如果已经存在角色，直接请求进入游戏
    entity.pid  = pkt.pid
    entity.name = pkt.name
    self:enter_world(entity)
end

-- 创角返回
function Loginout:on_create_role( entity,errno,pkt )
    if 0 ~= errno then
        PRINTF( "android_%d unable to create role",entity.index )
        return
    end

    entity.pid  = pkt.pid
    entity.name = pkt.name

    self:enter_world()

    PRINTF( "android_%d create role success,pid = %d,name = %s",
        entity.index,entity.pid,entity.name )
end

-- 请求进入游戏
function Loginout:enter_world( entity )
    entity:send_pkt( CS.PLAYER_ENTER,{} )
end

-- 确认进入游戏完成
function Loginout:on_enter_world( entity,errno,pkt )
    entity.ai.state = AST.ON

    -- 记录登录时间，一段时间后自动退出
    entity.ai.login_time = ev:time()

    PRINTF( "%s enter world success",entity.name )
    -- g_player_ev:fire_event( PLAYER_EV.ENTER,entity )
end

-- 初始化场景属性
function Loginout:on_init_property( entity,errno,pkt )
    ASSERT( nil == entity.handle,"on_init_property already have handle")

    entity.handle = pkt.handle
end

-- 被顶号
function Loginout:on_login_otherwhere( entity,errno,pkt )
    PRINTF( "%s login other where",entity.name )
end

-- ************************************************************************** --

-- 是否执行退出
function Loginout:check_and_logout(ai)
    local param = ai.ai_conf.param
    if (ai.login_time or 0) + param.logout_time > ev:time() then
        return false
    end

    ai.state = AST.OFF
    ai.logout_time = ev:time()

    local entity = ai.entity
    network_mgr:close( entity.conn_id )

    entity:set_conn( nil )
    PRINTF( "%s logout",entity.name )

    return true
end

-- ************************************************************************** --

-- 连接回调
function Loginout:on_conn_new(entity,conn_id,ecode)
    if 0 == ecode then
        PRINTF( "android(%d) connection(%d) establish",entity.index,conn_id)
        return
    end

    entity:set_conn(nil)

    ELOG( "android(%d) connection(%d) fail:%s",
        entity.index,conn_id,util.what_error(ecode) )
end

-- websocket握手回调
function Loginout:on_handshake(entity,conn_id,ecode)
    PRINTF( "android(%d) handshake ",entity.index)

    return self:do_login(entity)
end

-- ************************************************************************** --

local function net_cb( cb )
    return function(conn_id,ecode)
        local android = g_android_mgr:get_android_by_conn(conn_id)
        return cb(Loginout,android,conn_id,ecode)
    end
end

local function cmd_cb( cmd,cb )
    local raw_cb = function (android,errno,pkt)
        return cb(Loginout,android,errno,pkt)
    end

    g_android_cmd:cmd_register( cmd,raw_cb )
end

g_android_cmd:conn_new_register(net_cb(Loginout.on_conn_new))
g_android_cmd:handshake_register(net_cb(Loginout.on_handshake))

cmd_cb( SC.PLAYER_LOGIN,Loginout.on_login)
cmd_cb( SC.PLAYER_CREATE,Loginout.on_create_role)
cmd_cb( SC.PLAYER_ENTER,Loginout.on_enter_world)
cmd_cb( SC.PLAYER_OTHER,Loginout.on_login_otherwhere)
cmd_cb( SC.ENTITY_PROPERTY,Loginout.on_init_property)

return Loginout
