require "lua.global.global"
require "lua.global.oo"
require "lua.global.table"

local net_mgr = require "lua.net.netmgr"
local Client = require "lua.android.net.client"

local function main()
    ev:set_net_ref( net_mgr,net_mgr.accept_event,net_mgr.read_event,
        net_mgr.disconnect_event,net_mgr.connected_event )

    for i = 0,100 do
        local client = Client()
        client:connect( "127.0.0.1",9997 )
        net_mgr:push( client )
    end

    oo.check()
    ev:run()
end

xpcall( main, __G__TRACKBACK__ )
