require "global.global"
require "global.oo"
require "global.table"
require "global.string"
require "obj_counter"
json = require "lua_parson"

function sig_handler( signum )
    if g_mysql_mgr   then g_mysql_mgr:stop()   end
    if g_mongodb_mgr then g_mongodb_mgr:stop() end

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
    ev:signal( 2 );
    ev:signal( 15 );

    -- require "example.code_performance"
    -- require "example.mt_performance"
    -- require "example.mongo_performance"
    require "example.mysql_performance"
    -- require "example.log_performance"
    -- require "example.http_performance"
    -- require "example.stream_performance"
    -- require "example.words_filter_performance"

    vd( obj_counter.dump() )
    ev:backend()
end

xpcall( main, __G__TRACKBACK__ )
