require "global.global"
require "global.oo"
require "global.table"
require "global.string"
require "global.name"

require "modules.event.system_event"

require "timer.timer"
require "global.test"

require "test.misc_test"
require "test.mt_test"
require "test.https_test"
require "test.websocket_test"
require "test.lua_codec_test"
require "test.pbc_codec_test"
--[[
require "test.grid_aoi_test"
require "test.list_aoi_test"
require "test.mongodb_test"
require "test.mysql_test"
require "test.log_test"
require "test.words_filter_test"
require "test.rank_test"
require "test.rpc_test"
require "test.protobuf_test"
require "test.flatbuffers_test"
require "test.timer_test"
]]

local function exec_test()
    local opts = g_app.opts
    g_app.name, g_app.filter, g_app.skip = opts.app, opts.filter, opts.skip

    t_setup({
        print = print,
        timer = {
            new = function(timeout, func)
                return Timer.timeout(timeout, func)
            end,

            del = function(timer_id)
                return Timer.stop(timer_id)
            end
        },
        -- 跳过这些测试
        -- ./start.sh test --skip='mongodb;sql'
        skip = g_app.skip,
        -- 过滤器，允许只执行一部分测试
        -- ./start.sh test --filter=https 只执行名字包含https的测试
        filter = g_app.filter,
        time_update = function()
            ev:time_update()
        end,
        clock = function()
            return ev:steady_clock()
        end
    })

    -- 随机一个session，部分功能用到，如测试协议派发时
    g_app.session = 0x10001

    -- vd( statistic.dump() )

    -- 一些模块初始化时有顺序要求，需要检测ready状态
    -- 测试时是按需加载，因此设为false
    g_app.ready = false
    t_run()
end

SE.reg(SE_READY, exec_test)
