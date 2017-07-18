-- 机器人管理

local cmd = require "command.sc_command"
SC,CS = cmd[1],cmd[2]

local Android = oo.refer( "android.android" )

local network_mgr = network_mgr
local Android_mgr = oo.singleton( nil,... )

function Android_mgr:__init()
    self.cmd     = {}
    self.conn    = {}
    self.android = {}

    network_mgr:load_schema( "pb" )
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
    for index = 1,2000 do
        local conn_id = 
            network_mgr:connect( "127.0.0.1",10002,network_mgr.CNT_CSCN )

        local android = Android( index,conn_id )
        self.conn[conn_id]  = android
        self.android[index] = android
    end
end

-- 注册指令处理
function Android_mgr:cmd_register( cmd_cfg,handler )
    local cfg = self.cmd[cmd_cfg[1]]
    if not cfg then
        PLOG( "Android_mgr:cmd_dispatcher no such cmd:%d",cmd_cfg[1] )
        return
    end

    cfg.handler = handler
end

local android_mgr = Android_mgr()

function cscn_connect_new( conn_id,ecode )
    local android = android_mgr.conn[conn_id]
    if 0 ~= ecode then
        android_mgr.conn[conn_id] = nil
        android_mgr.android[android.index] = nil

        ELOG( "android(%d) connect fail:%s",
            android.index,util.what_error(ecode) )

        return
    end
    PLOG( "android(%d) connect establish",android.index)
    android:on_connect()
end

function sc_command_new( conn_id,cmd,errno,... )
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
        ELOG( "sc_command_new no handler found:%d",cmd )
        return
    end

    cfg.handler( android,errno,... )
end

function cscn_connect_del( conn_id )
    local android = android_mgr.conn[conn_id]

    android_mgr.conn[conn_id] = nil
    android_mgr.android[android.index] = nil

    ELOG( "android(%d) connect del",android.index )

    android:on_die()
end

return android_mgr


