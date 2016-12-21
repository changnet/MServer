
local SS = SS
local message_mgr = require "message/message_mgr"
local network_mgr = require "network/network_mgr"

-- 服务器同步
local function srv_syn( srv_conn,pkt )
    if not network_mgr:srv_register( srv_conn,pkt ) then return false end
    if not message_mgr:do_srv_register( srv_conn,pkt  ) then return false end

    srv_conn:authorized( pkt.session )

    local _pkt = network_mgr:register_pkt( message_mgr )
    message_mgr:srv_send( srv_conn,SS.SYS_ACK,_pkt )

    PLOG( "server(%#.8X) register succes",pkt.session )
end

-- 服务器同步返回
local function srv_ack( srv_conn,pkt )
    if not network_mgr:srv_register( srv_conn,pkt ) then return false end
    if not message_mgr:do_srv_register( srv_conn,pkt  ) then return false end

    srv_conn:authorized( pkt.session )

    PLOG( "server(%#.8X) register succes",pkt.session )
end

-- 这里注册系统模块的协议处理
message_mgr:srv_register( SS.SYS_SYN,srv_syn,true )
message_mgr:srv_register( SS.SYS_ACK,srv_ack,true )
