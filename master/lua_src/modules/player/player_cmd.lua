-- 玩家相关协议处理

local CS = CS
local SC = SC

local g_command_mgr = g_command_mgr
local g_network_mgr = g_network_mgr
local g_player_mgr  = g_player_mgr

local Base = require "modules.player.base"
local Player = require "modules.player.player"

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

local function player_base_cb( cb_func )
    return function( player,... )
        return cb_func( player:get_module( "base" ),... )
    end
end

-- 强制玩家断开连接，网关用
local function player_disconnect( pid )
    PFLOG( "kill player conn:%d",pid )

    g_account_mgr:role_offline_by_pid( pid )
    g_network_mgr:clt_close_by_pid( pid )
end

-- 这里注册系统模块的协议处理
if "gateway" == g_app.srvname then
    g_rpc:declare( "player_disconnect",player_disconnect )
    account_mgr_clt_cb( CS.PLAYER_LOGIN,g_account_mgr.player_login,true )
    account_mgr_clt_cb( CS.PLAYER_CREATE,g_account_mgr.create_role,true )
end

if "world" == g_app.srvname then
    g_command_mgr:clt_register( CS.PLAYER_PING,player_ping )
    player_mgr_clt_cb( CS.PLAYER_ENTER,g_player_mgr.on_enter_world )

    player_mgr_srv_cb( SS.PLAYER_OFFLINE,g_player_mgr.on_player_offline )
    player_mgr_srv_cb( SS.PLAYER_OTHERWHERE,g_player_mgr.on_login_otherwhere )

    g_player_ev:register( PLAYER_EV.ENTER,player_base_cb( Base.send_info ) )
    g_res:reg_player_res(
        RES.GOLD,Player.get_gold,Player.add_gold,Player.dec_gold )
end

