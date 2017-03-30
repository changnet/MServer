
local SS = SS
local command_mgr = require "command/command_mgr"
local network_mgr = require "network/network_mgr"

local rpc = require "rpc/rpc"

-- 收到另一个服务器主动同步
local function srv_syn( srv_conn,pkt )
    if not network_mgr:srv_register( srv_conn,pkt ) then return false end
    if not command_mgr:do_srv_register( srv_conn,pkt  ) then return false end

    srv_conn:authorized( pkt.session )

    local _pkt = network_mgr:register_pkt( command_mgr )
    command_mgr:srv_send( srv_conn,SS.SYS_ACK,_pkt )

    PLOG( "%s register succes",srv_conn:conn_name() )

    Main.one_wait_finish( pkt.name,1 )
end

-- 自己主动同步对方，对方服务器返回同步信息
local function srv_ack( srv_conn,pkt )
    if not network_mgr:srv_register( srv_conn,pkt ) then return false end
    if not command_mgr:do_srv_register( srv_conn,pkt  ) then return false end

    srv_conn:authorized( pkt.session )

    PLOG( "%s register succes",srv_conn:conn_name() )

    Main.one_wait_finish( pkt.name,1 )
end

-- 心跳包
local function srv_beat( srv_conn,pkt )
    if pkt.response then
        command_mgr:srv_send( srv_conn,SS.SYS_BEAT,{response = 1} )
    end

    -- 在这里不用更新自己的心跳，因为在on_command里已自动更新
end

-- 这里注册系统模块的协议处理
command_mgr:srv_register( SS.SYS_BEAT,srv_beat,true,false )

command_mgr:srv_register( SS.SYS_SYN,srv_syn,true,true )
command_mgr:srv_register( SS.SYS_ACK,srv_ack,true,true )

command_mgr:srv_register( SS.RPC_REQ,rpc.dispatch,true,false,true )
command_mgr:srv_register( SS.RPC_RES,rpc.response,true,false,true )

if Main.srvname == "gateway" then
    command_mgr:srv_register( SS.CLT_CMD,command_mgr.ssc_tansport,true,false,true )
end


if Main.srvname == "world" then
    command_mgr:srv_register( SS.CLT_CMD,
        command_mgr.css_dispatcher( command_mgr ),true,false,true )

    -------------- test rpc --------------------------------
    function rpc_echo( ... )
        print( "rpc echo",... )

        return 1,2,3,"test",nil,9989986547.66589
    end

    rpc:declare( "rpc_echo",rpc_echo )
end