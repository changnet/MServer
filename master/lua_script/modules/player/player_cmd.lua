-- 玩家相关协议处理

local CS = CS
local SC = SC

local g_command_mgr = g_command_mgr
local g_network_mgr = g_network_mgr
local g_player_mgr  = g_player_mgr

local function player_ping( srv_conn,pid,pkt )
    srv_conn:send_clt_pkt( pid,SC.PLAYER_PING,pkt )
end

local function account_mgr_clt_cb( cmd,cb_func,noauth )
    local cb = function( clt_conn,pkt )
        return cb_func( g_account_mgr,clt_conn,pkt )
    end

    g_command_mgr:clt_register( cmd,cb,noauth )
end

local function player_mgr_clt_cb( cmd,cb_func )
    local cb = function( clt_conn,pid,pkt )
        return cb_func( g_player_mgr,clt_conn,pid,pkt )
    end

    g_command_mgr:clt_register( cmd,cb )
end

local function player_mgr_srv_cb( cmd,cb_func )
    local cb = function( srv_conn,pkt )
        return cb_func( g_player_mgr,srv_conn,pkt )
    end

    g_command_mgr:srv_register( cmd,cb )
end

local function rpc_test( ... )
    print("rpc_test ==================================",...)
end

local function x_rpc_test( ... )
    print("x_rpc_test ==================================",...)
    return 1,3,5,90.1246,nil,{a = 5,b = 5},false,true,998
end

-- 这里注册系统模块的协议处理
if "gateway" == g_app.srvname then
    account_mgr_clt_cb( CS.PLAYER_LOGIN,g_account_mgr.player_login,true )
    account_mgr_clt_cb( CS.PLAYER_CREATE,g_account_mgr.create_role,true )
end

if "world" == g_app.srvname then
    g_command_mgr:clt_register( CS.PLAYER_PING,player_ping )
    player_mgr_clt_cb( CS.PLAYER_ENTER,g_player_mgr.on_enter_world )

    player_mgr_srv_cb( SS.PLAYER_OFFLINE,g_player_mgr.on_player_offline )
    player_mgr_srv_cb( SS.PLAYER_OTHERWHERE,g_player_mgr.on_login_otherwhere )

    g_rpc:declare("rpc_test",rpc_test)
    g_rpc:declare("x_rpc_test",x_rpc_test)
end
