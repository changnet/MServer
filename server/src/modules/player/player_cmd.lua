-- 玩家相关协议处理
local g_network_mgr = g_network_mgr
local g_player_mgr = g_player_mgr

local Base = require "modules.player.base"
local Player = require "modules.player.player"

local function account_mgr_clt_cb(cmd, cb_func, noauth)
    local cb = function(clt_conn, pkt)
        return cb_func(g_account_mgr, clt_conn, pkt)
    end

    Cmd.reg(cmd, cb, noauth)
end

local function player_mgr_clt_cb(cmd, cb_func, noauth)
    local cb = function(clt_conn, pid, pkt)
        return cb_func(g_player_mgr, clt_conn, pid, pkt)
    end

    Cmd.reg(cmd, cb, noauth)
end

local function player_mgr_srv_cb(cmd, cb_func)
    local cb = function(srv_conn, pkt)
        return cb_func(g_player_mgr, srv_conn, pkt)
    end

    Cmd.reg_srv(cmd, cb)
end

-- 基础模块回调
local function player_base_cb(cb_func)
    return function(player, ...)
        return cb_func(player:get_module("base"), ...)
    end
end

-- 玩家对象回调
local function player_clt_cb(cmd, cb_func)
    local cb = function(srv_conn, pid, pkt)
        local player = g_player_mgr:get_player(pid)
        return cb_func(player, pkt)
    end

    Cmd.reg(cmd, cb)
end

-- 强制玩家断开连接，网关用
local function kill_player_connect(pid)
    PRINTF("kill player conn:%d", pid)

    g_account_mgr:role_offline_by_pid(pid)
    g_network_mgr:clt_close_by_pid(pid)
end

-- 这里注册系统模块的协议处理
if GATEWAY == APP_TYPE then
    reg_func("kill_player_connect", kill_player_connect)
    account_mgr_clt_cb(PLAYER.LOGIN, g_account_mgr.player_login, true)
    account_mgr_clt_cb(PLAYER.CREATE, g_account_mgr.create_role, true)
end

if WORLD == APP_TYPE then
    player_clt_cb(PLAYER.ENTERDUNGEON, Player.enter_dungeon)
    player_mgr_clt_cb(PLAYER.ENTER, g_player_mgr.on_enter_world, true)

    player_mgr_srv_cb(SYS.PLAYER_OFFLINE, g_player_mgr.on_player_offline)
    player_mgr_srv_cb(SYS.PLAYER_OTHERWHERE, g_player_mgr.on_login_otherwhere)

    PE.reg(PE_ENTER, player_base_cb(Base.send_info))
    g_res:reg_player_res(RES.GOLD, Player.get_gold, Player.add_gold,
                         Player.dec_gold)
end

