require "global.global"
require "global.oo"
require "global.table"
require "global.string"

local client_mgr = require "net.client_mgr"
local Store_sql = require "sql.store_sql"

local function sig_handler( signum )
    g_store_sql:stop()
    ev:exit()
end

local function sql_cb()
    print( "sql callback ========================================" )
end

g_store_sql = Store_sql()
local function main()
    ev:set_signal_ref( sig_handler )
    ev:signal( 2 );
    ev:signal( 15 );

    client_mgr:listen( "0.0.0.0",9997 )

    g_store_sql:start( "192.168.1.23",3306,"root","123456","xzc_mudrv" )
    -- for i = 1,102400 do
    --     local str = string.format("insert into new_table (name,money,gold) values \
    --         ('xzc%d',%d,%d)",i,i*10,i*100 );
    --     g_store_sql:do_sql( str )
    -- end

    oo.check()
    ev:run()
end

xpcall( main, __G__TRACKBACK__ )
