require "global.global"
require "global.oo"
require "global.table"
require "global.string"
require "obj_counter"
json = require "lua_parson"

local function sig_handler( signum )
    if g_store_mongo then g_store_mongo:stop() end
    if g_store_sql then g_store_sql:stop() end
    if g_log_mgr then g_log_mgr:stop(); end
    ev:exit()
end

function socket_accpet_new( conn_id )
    print( "socket_accpet_new",conn_id )
end

function socket_connect( conn_id,errno )
    print( "socket_connect",conn_id,errno )
end

local function main()
    ev:set_signal_ref( sig_handler )
    ev:signal( 2 );
    ev:signal( 15 );

    -- require "example.code_performance"
    -- require "example.mt_performance"
    -- require "example.mongo_performance"
    -- require "example.mysql_performance"
    -- require "example.log_performance"
    -- require "example.http_performance"
    -- require "example.stream_performance"
    -- require "example.words_filter_performance"

    network_mgr:listen( "127.0.0.1",9999,1 )
    network_mgr:connect( "127.0.0.1",9999,1 )

    print( obj_counter.obj_count("Http_socket") )
    oo.check()
    vd( obj_counter.dump() )
    ev:backend()
end

xpcall( main, __G__TRACKBACK__ )
