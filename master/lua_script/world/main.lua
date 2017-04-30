require "global.global"
require "global.oo"
require "global.table"
require "global.string"

require "global.define"

-- 协议使用太频繁，放到全局变量
require "command.ss_command"
local CMD = require "command.sc_command"

-- 使用oo的define功能让这两个表local后仍能热更
SC = oo.define( CMD[1],"command_sc" )
CS = oo.define( CMD[2],"command_cs" )

Main = {}       -- store dynamic runtime info to global
Main.command,Main.srvname,Main.srvindex,Main.srvid = ...

Main.wait = 
{
    gateway = 1,
}

local Unique_id = require "global.unique_id"

g_unique_id = Unique_id()
Main.session = g_unique_id:srv_session(
    Main.srvname,tonumber(Main.srvindex),tonumber(Main.srvid) )

g_setting     = require "world.setting"
g_rpc         = require "rpc.rpc"
g_network_mgr = require "network.network_mgr"
g_command_mgr = require "command.command_mgr"
g_player_mgr  = require "player.player_mgr"

require "command/command_header"

local Srv_conn    = require "network.srv_conn"

function Main.sig_handler( signum )
    if g_store_mongo then g_store_mongo:stop() end
    if   g_store_sql then   g_store_sql:stop() end
    if     g_log_mgr then     g_log_mgr:stop() end

    ev:exit()
end

-- 检查需要等待的服务器是否初始化完成
function Main.one_wait_finish( name,val )
    if not Main.wait[name] then return end

    Main.wait[name] = Main.wait[name] - val
    if Main.wait[name] <= 0 then Main.wait[name] = nil end

    if table.empty( Main.wait ) then Main.final_init() end
end

function Main.init()
    Main.starttime = ev:time()
    network_mgr:set_curr_session( Main.session )

    local fs = g_command_mgr:load_schema()
    PLOG( "world load flatbuffers schema:%d",fs )

    if not g_network_mgr:srv_listen( g_setting.sip,g_setting.sport ) then
        ELOG( "world server listen fail,exit" )
        os.exit( 1 )
    end

    g_network_mgr:connect_srv( g_setting.servers )
end

function Main.final_init()
    Main.ok = true
    PLOG( "world server(0x%.8X) start OK",Main.session )
end

local function main()
    ev:set_signal_ref( Main.sig_handler )
    ev:signal( 2  )
    ev:signal( 15 )

    Main.init()

    ev:backend()
end

xpcall( main, __G__TRACKBACK__ )
