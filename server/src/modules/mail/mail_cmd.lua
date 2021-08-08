-- mail_cmd.lua
-- 2018-05-20
-- xzc
-- 邮件指令入口
local g_mail_mgr = g_mail_mgr
local g_player_mgr = g_player_mgr

local Mail = require "modules.mail.mail"

-- 邮件回调
-- local function mail_cb( cmd,func )
--     local cb = function( srv_conn,pid,pkt )
--         local player = PlayerMgr.get_player( pid )
--         return func( player:get_module("mail"),pkt )
--     end

--     Cmd.reg( cmd,cb )
-- end

-- 邮件管理器回调
local function mail_mgr_cb(cmd, func)
    local cb = function(srv_conn, pid, pkt)
        local player = PlayerMgr.get_player(pid)
        return func(g_mail_mgr, player, pkt)
    end

    Cmd.reg(cmd, cb)
end

-- 其他服发邮件时通过rpc发送个人邮件到world进程处理
local function rpc_send_mail(pid, title, ctx, attachment, op)
    g_mail_mgr:raw_send_mail(pid, title, ctx, attachment, op)
end

-- 其他服发邮件时通过rpc发送系统邮件到world进程处理
local function rpc_send_sys_mail(title, ctx, attachment, op, expire, level, vip)
    g_mail_mgr:raw_send_sys_mail(title, ctx, attachment, op, expire, level, vip)
end

if WORLD == APP_TYPE then
    reg_func("rpc_send_mail", rpc_send_mail)
    reg_func("rpc_send_sys_mail", rpc_send_sys_mail)

    mail_mgr_cb(MAIL.DEL, Mail.handle_mail_del)
end
