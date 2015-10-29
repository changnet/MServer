require "global.global"
require "global.oo"
require "global.table"

-- local client_mgr = require "net.clientmgr"
-- local net_mgr = require "net.netmgr"
-- local timer_mgr = require "timer.timermgr"
require "signal.signal"

local function main()
    --client_mgr:listen( "0.0.0.0",9997 )
    print( "time:",ev:time() )
    oo.check()
    ev:run()
end

xpcall( main, __G__TRACKBACK__ )
