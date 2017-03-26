-- 玩家相关协议处理

local CS = CS
local SC = SC

local command_mgr = require "command/command_mgr"
local network_mgr = require "network/network_mgr"

local function player_login( clt_conn,pkt )
    clt_conn:authorized()

    command_mgr:clt_send( clt_conn,SC.PLAYER_LOGIN,{name="TODO"} )

    PLOG( "client authorized success:%s",pkt.account )
end

local function player_ping( clt_conn,pkt )
    PLOG( "ping ====>>>> " )
end

-- 这里注册系统模块的协议处理
if "gateway" == Main.srvname then
    command_mgr:clt_register( CS.PLAYER_LOGIN,player_login,true )
end

if "world" == Main.srvname then
    command_mgr:clt_register( CS.PLAYER_PING,player_ping )
end