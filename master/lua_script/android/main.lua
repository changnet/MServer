require "global.global"
require "global.oo"
require "global.table"

math.randomseed( ev:time() )

require "signal.signal"

local function main()
    require "android.stream_client_test"

    oo.check()
    ev:run()
end

xpcall( main, __G__TRACKBACK__ )
