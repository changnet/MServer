-- 机器人管理

local handshake_clt = table.concat(
{
    'GET / HTTP/1.1\r\n',
    'Connection: Upgrade\r\n',
    'Sec-WebSocket-Key: %s\r\n',
    'Upgrade: websocket\r\n',
    'Sec-WebSocket-Version: 13\r\n\r\n',
} )

local cmd = require "proto.proto"
SC,CS = cmd[1],cmd[2]

local util = require "util"
require "modules.system.define"
local Android = require "android.android"

local network_mgr = network_mgr
local Android_mgr = oo.singleton( nil,... )

function Android_mgr:__init()
    self.cmd     = {}
    self.conn    = {}
    self.android = {}

    local fs = network_mgr:load_one_schema( network_mgr.CDC_PROTOBUF,"pb" )
    PFLOG( "android load protocol schema:%d",fs )
    for _,v in pairs( SC or {} ) do
        self.cmd[ v[1] ] = v
    end
end

function Android_mgr:init_sc_command( list )
    for _,cfg in pairs( list ) do
        network_mgr:set_sc_cmd( cfg[1],cfg[2],cfg[3],0,0 )
    end
end

function Android_mgr:init_cs_command( list )
    for _,cfg in pairs( list ) do
        network_mgr:set_cs_cmd( cfg[1],cfg[2],cfg[3],0,0 )
    end
end

function Android_mgr:start()
    local srvindex = tonumber(g_app.srvindex) -- 平台
    local srvid = tonumber(g_app.srvid) -- 服务器
    for index = 1,1 do
        local conn_id = 
            network_mgr:connect( "127.0.0.1",10002,network_mgr.CNT_CSCN )

        assert( nil == self.conn[conn_id] )
        local idx = ( srvid << 16 ) | index
        local android = Android( idx,conn_id )
        self.conn[conn_id]  = android
        self.android[idx] = android
    end
end

-- 注册指令处理
function Android_mgr:cmd_register( cmd_cfg,handler )
    local cfg = self.cmd[cmd_cfg[1]]
    if not cfg then
        PFLOG( "Android_mgr:cmd_dispatcher no such cmd:%d",cmd_cfg[1] )
        return
    end

    cfg.handler = handler
end

function Android_mgr:dump_pkt( pkt )
    vd(pkt)
end

local android_mgr = Android_mgr()

function handshake_new( conn_id,sec_websocket_key,sec_websocket_accept )
    if not sec_websocket_accept then
        print("handshake no sec_websocket_accept")
        return
    end

    -- TODO:验证sec_websocket_accept是否正确
    print( "clt handshake",sec_websocket_accept)
    local android = android_mgr.conn[conn_id]
    android:on_connect()
end

function conn_new( conn_id,ecode )
    local android = android_mgr.conn[conn_id]
    if 0 ~= ecode then
        android_mgr.conn[conn_id] = nil
        android_mgr.android[android.index] = nil

        ELOG( "android(%d) connect fail:%s",
            android.index,util.what_error(ecode) )

        return
    end

    network_mgr:set_conn_io( conn_id,network_mgr.IOT_NONE )
    network_mgr:set_conn_codec( conn_id,network_mgr.CDC_PROTOBUF )
    network_mgr:set_conn_packet( conn_id,network_mgr.PKT_WSSTREAM )

    -- 主动发送握手
    local sec_websocket_key = util.base64( util.uuid() ) -- RFC6455 随机一个key
    network_mgr:send_raw_packet( 
        conn_id,string.format(handshake_clt,sec_websocket_key) )

    PFLOG( "android(%d) connect establish",android.index)

    -- 握手完成之前不要发数据，因为发送的模式不对，是以http发送的
end

function command_new( conn_id,cmd,errno,... )
    local android = android_mgr.conn[conn_id]
    if not android then
        ELOG( "sc_command_new no connect found" )
    end

    local cfg = android_mgr.cmd[cmd]
    if not cfg then
        ELOG( "sc_command_new no such cmd:%d",cmd )
        return
    end

    if not cfg.handler then
        android_mgr:dump_pkt( ... )
        ELOG( "sc_command_new no handler found:%d",cmd )
        return
    end

    cfg.handler( android,errno,... )
end

function conn_del( conn_id )
    local android = android_mgr.conn[conn_id]

    android_mgr.conn[conn_id] = nil
    android_mgr.android[android.index] = nil

    ELOG( "android(%d) connect del",android.index )

    android:on_die()
end

return android_mgr


