require "global.global"
require "global.oo"
require "global.table"
require "global.define"

local g_android_mgr = require "android.android_mgr"

require "android.android"
require "android.android_cmd"

math.randomseed( ev:time() )

function sig_handler( signum )
    ev:exit()
end

local function main()
    ev:signal( 2 );
    ev:signal( 15 );

    g_android_mgr:start()

    ev:backend()
end

xpcall( main, __G__TRACKBACK__ )
