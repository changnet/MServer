require "lua.global.global"
require "lua.global.oo"
require "lua.global.table"

local client_mgr = require "lua.net.clientmgr"
local net_mgr = require "lua.net.netmgr"
local timer_mgr = require "lua.timer.timermgr"

local id = nil
local cnt = 0

function t( a,b,c )
    print( a,b,c)
    cnt = cnt + 1
    if cnt >= 10 then
        timer_mgr:stop( id )
    end
end

local function main()
    client_mgr:listen( "0.0.0.0",9997 )
    id = timer_mgr:start( 1,t,1,5,90 )

    oo.check()
    ev:run()
end

xpcall( main, __G__TRACKBACK__ )
