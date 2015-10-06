require "lua.global.global"
require "lua.global.oo"
require "lua.global.table"

math.randomseed( ev:time() )
local net_mgr = require "lua.net.netmgr"
local Android = require "lua.android.playerdata.android"
require "lua.signal.signal"

local function main()
    local androids = {}
    
    for i = 1,900 do
        local android = Android(i)
        android:born( "127.0.0.1",9997 )
        
        table.insert( androids,android )
    end

    oo.check()
    ev:run()
end

xpcall( main, __G__TRACKBACK__ )
