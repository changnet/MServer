require "lua.global.global"
require "lua.global.oo"
require "lua.global.table"

local client_mgr = require "lua.net.clientmgr"
local net_mgr = require "lua.net.netmgr"
local timer_mgr = require "lua.timer.timermgr"
require "lua.signal.signal"

local function main()
    client_mgr:listen( "0.0.0.0",9997 )

    oo.check()
    ev:run()
end

xpcall( main, __G__TRACKBACK__ )
