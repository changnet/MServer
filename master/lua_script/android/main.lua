require "global.global"
require "global.oo"
require "global.table"


local android_mgr = require "android/android_mgr"

require "android/android"
require "android/android_cmd"

math.randomseed( ev:time() )

local function sig_handler( signum )
    ev:exit()
end

local function main()
    ev:set_signal_ref( sig_handler )
    ev:signal( 2 );
    ev:signal( 15 );

    android_mgr:start()

    ev:backend()
end

xpcall( main, __G__TRACKBACK__ )
