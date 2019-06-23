-- 机器人指令处理入口
-- 机器人的网络模块极其简单，也不考虑热更之类，在这里简单处理了

local handshake_clt = table.concat(
{
    'GET / HTTP/1.1\r\n',
    'Connection: Upgrade\r\n',
    'Sec-WebSocket-Key: %s\r\n',
    'Upgrade: websocket\r\n',
    'Sec-WebSocket-Version: 13\r\n\r\n',
} )

local sc_cmd = {}
local conn_new_handler = {}
local handshake_handler = {}
local network_mgr = network_mgr

do
    local sc = require "proto.proto"
    SC,CS = sc[1],sc[2]

    local fs = network_mgr:load_one_schema( network_mgr.CDC_PROTOBUF,"pb" )
    PRINTF( "android load protocol schema:%d",fs )

    -- 注册服务器发往机器人的指令配置
    for _,cfg in pairs( SC ) do
        sc_cmd[ cfg[1] ] = cfg
        network_mgr:set_sc_cmd( cfg[1],cfg[2],cfg[3],0,0 )
    end

    -- 注册机器人发往服务器的指令配置
    for _,cfg in pairs( CS ) do
        network_mgr:set_cs_cmd( cfg[1],cfg[2],cfg[3],0,0 )
    end
end

local Android_cmd = oo.singleton( ... )

-- 注册指令处理
function Android_cmd:cmd_register( cmd_cfg,handler )
    local cfg = sc_cmd[cmd_cfg[1]]
    if not cfg then
        PRINTF( "Android_cmd:cmd_dispatcher no such cmd:%d",cmd_cfg[1] )
        return
    end

    cfg.handler = handler
end

-- 握手处理
function Android_cmd:handshake_register( handler )
    handshake_handler = handler
end

-- 连接成功，回调对应的模块
function Android_cmd:conn_new_register( handler )
    conn_new_handler = handler
end

function Android_cmd:dump_pkt( pkt )
    vd(pkt)
end

local android_cmd = Android_cmd()

function handshake_new( conn_id,sec_websocket_key,sec_websocket_accept )
    if not sec_websocket_accept then
        print("handshake no sec_websocket_accept")
        return
    end

    -- TODO:验证sec_websocket_accept是否正确

    if handshake_handler then handshake_handler(conn_id,0) end
end

function conn_new( conn_id,ecode )
    if 0 ~= ecode then
        if conn_new_handler then conn_new_handler(conn_id,ecode) end

        return
    end

    network_mgr:set_conn_io( conn_id,network_mgr.IOT_NONE )
    network_mgr:set_conn_codec( conn_id,network_mgr.CDC_PROTOBUF )

    -- 使用tcp二进制流
    network_mgr:set_conn_packet( conn_id,network_mgr.PKT_STREAM )

    -- 使用websocket二进制流
    -- network_mgr:set_conn_packet( conn_id,network_mgr.PKT_WSSTREAM )

    -- -- 主动发送握手
    -- local sec_websocket_key = util.base64( util.uuid() ) -- RFC6455 随机一个key
    -- network_mgr:send_raw_packet(
    --     conn_id,string.format(handshake_clt,sec_websocket_key) )

    -- 握手完成之前不要发数据，因为发送的模式不对，是以http发送的
    if conn_new_handler then conn_new_handler(conn_id,ecode) end
end

function command_new( conn_id,cmd,errno,... )
    local cfg = sc_cmd[cmd]
    if not cfg then
        ERROR( "sc_command_new no such cmd:%d",cmd )
        return
    end

    if not cfg.handler then
        -- android_cmd:dump_pkt( ... )
        ERROR( "sc_command_new no handler found:%d",cmd )
        return
    end

    local android = g_android_mgr:get_android_by_conn(conn_id)
    cfg.handler( android,errno,... )
end

function conn_del( conn_id )
    PRINT( "%d connect del",android.index )
end

return android_cmd
