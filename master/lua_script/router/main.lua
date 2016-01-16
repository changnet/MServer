require "global.global"
require "global.oo"
require "global.table"
require "global.string"

local client_mgr  = require "net.client_mgr"
local Store_sql   = require "sql.store_sql"
local Store_mongo = require "mongo.store_mongo"

local function sig_handler( signum )
    g_store_mongo:stop()
    --g_store_sql:stop()
    ev:exit()
end

g_store_sql = Store_sql()
g_store_mongo = Store_mongo()
local function main()
    ev:set_signal_ref( sig_handler )
    ev:signal( 2 );
    ev:signal( 15 );

    client_mgr:listen( "0.0.0.0",9997 )

    --g_store_sql:start( "127.0.0.1",3306,"root","123456","xzc_mudrv" )
    g_store_mongo:start( "127.0.0.1",27013,"test","test","mudrv" )
    g_store_mongo:count( "item" )
    g_store_mongo:find( "item" )
    g_store_mongo:find_and_modify( "item",'{"_id":2009}',nil,'{"$set":{"amount":90}}' )

    local info = {}
    info.amount = 889
    info.ext    = "OK"
    info.list = table.array( {1,2,3,4,5,6,7,99,"abc",true,false} )

    local query = {}
    query._id   = 123456781
    g_store_mongo:remove( "item",query )
    -- for i = 1,102400 do
    --     local str = string.format("insert into new_table (name,money,gold) values \
    --         ('xzc%d',%d,%d)",i,i*10,i*100 );
    --     g_store_sql:do_sql( str )
    -- end

    oo.check()
    ev:run()
end

xpcall( main, __G__TRACKBACK__ )
