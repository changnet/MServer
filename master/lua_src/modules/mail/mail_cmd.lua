-- mail_cmd.lua
-- 2018-05-20
-- xzc

-- 邮件指令入口

local g_mail_mgr = g_mail_mgr
local g_player_mgr = g_player_mgr

local function mail_mgr_cb( cmd,func )
    local cb = function( srv_conn,pid,pkt )
        local player = g_player_mgr:get_player( pid )
        return func( g_mail_mgr,player,pkt )
    end

    g_command_mgr:clt_register( cmd,cb )
end

-- 其他服发邮件时通过rpc发送到world进程处理
local function rpc_mail()
end

if "world" == g_app.srvname then
    g_rpc:declare( "rpc_mail",rpc_mail )
end
