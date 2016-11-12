require "global.global"
require "global.oo"
require "global.table"
require "global.string"

local setting = require "gateway/setting"
local network_mgr = require "network/network_mgr"
local message_mgr = require "message/message_mgr"

local argv = { ... }

function sig_handler( signum )
    if g_store_mongo then g_store_mongo:stop() end
    if   g_store_sql then   g_store_sql:stop() end
    if     g_log_mgr then     g_log_mgr:stop() end

    ev:exit()
end

function pre_init()
    local session = network_mgr:generate_srv_session(
        argv[2],tonumber(argv[3]),tonumber(argv[4]) )

    message_mgr:init_message()

    if not network_mgr:srv_listen( setting.sip,setting.sport ) then
        ELOG( "server listen fail,exit" )
        os.exit( 1 )
    end
end

function main()
    ev:set_signal_ref( sig_handler )
    ev:signal( 2 );
    ev:signal( 15 );

    pre_init();

    ev:run()
end

xpcall( main, __G__TRACKBACK__ )
