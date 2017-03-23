
local SS = SS
local command_mgr = require "command/command_mgr"
local network_mgr = require "network/network_mgr"

-- 收到另一个服务器主动同步
local function srv_syn( srv_conn,pkt )
    if not network_mgr:srv_register( srv_conn,pkt ) then return false end
    if not command_mgr:do_srv_register( srv_conn,pkt  ) then return false end

    srv_conn:authorized( pkt.session )

    local _pkt = network_mgr:register_pkt( command_mgr )
    command_mgr:srv_send( srv_conn,SS.SYS_ACK,_pkt )

    PLOG( "server(%#.8X) register succes",pkt.session )

    Main.one_wait_finish( pkt.name,1 )
end

-- 自己主动同步对方，对方服务器返回同步信息
local function srv_ack( srv_conn,pkt )
    if not network_mgr:srv_register( srv_conn,pkt ) then return false end
    if not command_mgr:do_srv_register( srv_conn,pkt  ) then return false end

    srv_conn:authorized( pkt.session )

    PLOG( "server(%#.8X) register succes",pkt.session )

    Main.one_wait_finish( pkt.name,1 )
end

-- 这里注册系统模块的协议处理
command_mgr:srv_register( SS.SYS_SYN,srv_syn,true,true )
command_mgr:srv_register( SS.SYS_ACK,srv_ack,true,true )
