require "global.global"
require "global.oo"
require "global.table"
require "global.define"

math.randomseed( ev:time() )

Main = {}       -- store dynamic runtime info to global
Main.command,Main.srvname,Main.srvindex,Main.srvid = ...

g_timer_mgr   = require "timer.timer_mgr"
local g_android_mgr = require "android.android_mgr"

require "android.android"
require "android.android_cmd"

g_ai_mgr = require "ai.ai_mgr"
g_player_ev = require "event.player_event"

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
