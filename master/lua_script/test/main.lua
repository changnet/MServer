require "global.global"
require "global.oo"
require "global.table"
require "global.string"
json = require "lua_parson"

local Http_mgr    = require "http.http_mgr"
local Http_client = require "http.http_client"

local function sig_handler( signum )
    if g_store_mongo then g_store_mongo:stop() end
    if g_store_sql then g_store_sql:stop() end
    if g_log_mgr then g_log_mgr:stop();g_log_mgr:join() end
    ev:exit()
end

g_http_mgr    = Http_mgr()
local function main()
    ev:set_signal_ref( sig_handler )
    ev:signal( 2 );
    ev:signal( 15 );

    require "example.code_performance"
    --require "example.mongo_performance"
    --require "example.mysql_performance"
    require "example.log_performance"

    g_http_mgr:listen( "0.0.0.0",8887 )

    oo.check()
    ev:run()
end

xpcall( main, __G__TRACKBACK__ )
