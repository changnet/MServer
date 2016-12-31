require "global.global"
require "global.oo"
require "global.table"
require "global.string"

Main = {}       -- store dynamic runtime info to global
Main.argv = { ... }

local setting     = require "gateway/setting"
local network_mgr = require "network/network_mgr"
local message_mgr = require "message/message_mgr"
local Srv_conn    = require "network/srv_conn"
local Clt_conn    = require "network/clt_conn"

local function sig_handler( signum )
    if g_store_mongo then g_store_mongo:stop() end
    if   g_store_sql then   g_store_sql:stop() end
    if     g_log_mgr then     g_log_mgr:stop() end

    ev:exit()
end

local function pre_init()
    Main.session = network_mgr:generate_srv_session(
        Main.argv[2],tonumber(Main.argv[3]),tonumber(Main.argv[4]) )

    message_mgr:init_message()
    local fs = message_mgr:load_schema()
    PLOG( "gateway load flatbuffers schema:%d",fs )

    if not network_mgr:srv_listen( setting.sip,setting.sport ) then
        ELOG( "server listen fail,exit" )
        os.exit( 1 )
    end

    if not network_mgr:clt_listen( setting.cip,setting.cport ) then
        ELOG( "server listen fail,exit" )
        os.exit( 1 )
    end
end

local function main()
    ev:set_signal_ref( sig_handler )
    ev:signal( 2 );
    ev:signal( 15 );

    pre_init();

    ev:backend()
end

xpcall( main, __G__TRACKBACK__ )
