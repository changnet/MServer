-- app.lua
-- 2018-04-04
-- xzc

-- android进程app

require "global.global"
require "global.oo"
require "global.table"
require "global.require_conf"

require "modules.system.define"
g_timer_mgr   = require "timer.timer_mgr"
g_android_cmd = require "android.android_cmd"
g_android_mgr = require "android.android_mgr"
-- g_player_ev = require "modules.event.player_event"

g_ai_mgr = require "modules.ai.ai_mgr"

-- 信号处理，默认情况下退出
function sig_handler( signum )
    ev:exit()
end

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

    ev:set_app_ev( 1000 ) -- 1000毫秒回调一次主循环
    ev:backend()
end

-- 主事件循环，设置了ev:set_app_ev后由C++回调
function application_ev( ms_now )
    g_android_mgr:routine( ms_now )
end

return App
