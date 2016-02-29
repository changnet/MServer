require "global.global"
require "global.oo"
require "global.table"
require "global.string"
json = require "global.json"

local Store_sql   = require "sql.store_sql"
local Store_mongo = require "mongo.store_mongo"
local Http_mgr    = require "http.http_mgr"

local function sig_handler( signum )
    --g_store_mongo:stop()
    --g_store_sql:stop()
    ev:exit()
end

g_store_sql   = Store_sql()
g_store_mongo = Store_mongo()
g_http_mgr    = Http_mgr()
local function main()
    ev:set_signal_ref( sig_handler )
    ev:signal( 2 );
    ev:signal( 15 );

    g_http_mgr:listen( "0.0.0.0",8887 )

    --g_store_sql:start( "127.0.0.1",3306,"root","123456","xzc_mudrv" )
    --g_store_mongo:start( "127.0.0.1",27013,"test","test","mudrv" )

    oo.check()
    ev:run()
end

xpcall( main, __G__TRACKBACK__ )
