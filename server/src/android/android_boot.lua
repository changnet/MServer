-- app.lua
-- 2018-04-04
-- xzc
-- android进程app

require "global.table"
require "global.math"
require "global.string"
require "global.time"
require "global.name"
require "timer.timer"
require "modules.event.system_event"
g_stat_mgr = require "statistic.statistic_mgr"

require "network.cmd"
require "android.android_mgr"

g_ai_mgr = require "modules.ai.ai_mgr"

local function exec_android()
    local opts = g_app.opts
    g_app.name = opts.name
    g_app.index = assert(opts.index, "miss argument --index")
    g_app.id = assert(opts.id, "miss argument --id")

    Cmd.USE_CS_CMD = true
    Cmd.load_protobuf()

    AndroidMgr.start()
end

-- 主事件循环，设置了ev:set_app_ev后由C++回调
local function android_ev(ms_now)
    AndroidMgr.routine(ms_now)
end

local function app_stop()
    AndroidMgr.stop()
    return true
end

App.reg_ev(android_ev)
App.reg_stop("android", app_stop)

SE.reg(SE_READY, exec_android)
