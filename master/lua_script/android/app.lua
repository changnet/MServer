-- app.lua
-- 2018-04-04
-- xzc

-- android进程app

require "global.global"
require "global.oo"
require "global.table"

g_timer_mgr   = require "timer.timer_mgr"
g_android_mgr = require "android.android_mgr"
g_ai_mgr = require "modules.ai.ai_mgr"
g_player_ev = require "event.player_event"

require "android.android_cmd"

local App = oo.class( nil,... )

-- 初始化
function App:__init( ... )
    self.command,self.srvname,self.srvindex,self.srvid = ...
end

-- 重写关服接口
function App:exec()
    ev:signal( 2 )
    ev:signal( 15 )

    g_android_mgr:start()

    ev:backend()
end

return App
