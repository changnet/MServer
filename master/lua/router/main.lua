require "lua.global.global"
require "lua.global.oo"
require "lua.global.table"

local listen = require "lua.net.listener"
local net_mgr = require "lua.net.netmgr"

local function main()
    ev:set_net_ref( net_mgr,net_mgr.event_cb )

    listen:listen( "0.0.0.0",9997 )
    net_mgr:push( listen )

    ev:run()
end

xpcall( main, __G__TRACKBACK__ )
