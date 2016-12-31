require "global.global"
require "global.oo"
require "global.table"
local Android = require "android/android"

math.randomseed( ev:time() )

local function sig_handler( signum )
    ev:exit()
end

local function main()
    ev:set_signal_ref( sig_handler )
    ev:signal( 2 );
    ev:signal( 15 );

    local android = Android( 1 )
    android:born( "127.0.0.1",10002 )

    ev:backend()
end

xpcall( main, __G__TRACKBACK__ )
