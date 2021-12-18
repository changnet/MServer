-- app.lua
-- 2018-04-04
-- xzc
-- android进程app
require "global.global"
require "global.oo"
require "global.table"
require "global.string"

require "modules.event.system_event"
require_define "modules.system.define"

-- 前置声明，避免luacheck警告

g_stat_mgr = nil
g_timer_mgr = nil
g_ai_mgr = nil

-- 信号处理，默认情况下退出
function sig_handler(signum)
    if signum == 15 then
        local json = require "engine.lua_parson"
        local stat = require "modules.system.statistic"

        local total_stat = stat.collect()

        local ctx = json.encode(total_stat)

        local of = io.open("log/android_stat", "a+")
        of:write(ctx)
        of:write("\n\n\n")
        of:close()
        return
    end

    ev:exit()
end

local function exec()
    ev:signal(2)
    ev:signal(15)

    local opts = g_app.opts
    g_app.name, g_app.index, g_app.id = opts.name, assert(
                                                   opts.index,
                                                   "miss argument --index"),
                                               assert(opts.id,
                                                      "miss argument --id")
    g_stat_mgr = require "statistic.statistic_mgr"

    -- 加载协议文件，在其他文件require之前加载，因为require的时候就需要注册协议
    require "network.cmd"
    Cmd.USE_CS_CMD = true
    Cmd.load_protobuf()

    g_timer_mgr = require "timer.timer_mgr"
    require "android.android_mgr"
    g_ai_mgr = require "modules.ai.ai_mgr"

    AndroidMgr.start()

    ev:set_app_ev(1000) -- 1000毫秒回调一次主循环
    ev:backend()
end

-- 主事件循环，设置了ev:set_app_ev后由C++回调
function application_ev(ms_now)
    AndroidMgr.routine(ms_now)
end

exec()
