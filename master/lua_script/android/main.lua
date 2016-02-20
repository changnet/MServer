require "global.global"
require "global.oo"
require "global.table"

math.randomseed( ev:time() )

local Android = require "android.playerdata.android"
require "signal.signal"

local function main()
    local androids = {}

    for i = 1,2048 do
        local android = Android(i)
        android:born( "127.0.0.1",9997 )

        table.insert( androids,android )
    end

    oo.check()
    ev:run()
end

xpcall( main, __G__TRACKBACK__ )
