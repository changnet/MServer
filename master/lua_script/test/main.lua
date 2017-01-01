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

    print( obj_counter.obj_count("Http_socket") )
    oo.check()
    vd( obj_counter.dump() )
    ev:backend()
end

xpcall( main, __G__TRACKBACK__ )
