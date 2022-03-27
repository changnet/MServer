require "global.global"
require "global.oo"
require "global.table"
require "global.string"
require "global.name"
require "engine.statistic"

require "modules.event.system_event"
json = require "engine.lua_parson"

require "timer.timer"

local function exec()
    require "global.test"

    local opts = g_app.opts
    g_app.name, g_app.filter, g_app.skip = opts.app, opts.filter,
                                                  opts.skip
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
            return ev:real_ms_time()
        end
    })

    -- 随机一个session，部分功能用到，如测试协议派发时
    g_app.session = 0x10001
    network_mgr:set_curr_session(g_app.session)

    -- test这个进程的逻辑不走app.lua中的初始化流程，这个函数结束，那整个进程就结束了

    require "test.misc_test"
    require "test.https_test"
    require "test.grid_aoi_test"
    require "test.list_aoi_test"
    require "test.mt_test"
    require "test.mongodb_test"
    require "test.mysql_test"
    require "test.log_test"
    -- require "example.stream_performance"
    require "test.websocket_test"
    require "test.words_filter_test"
    require "test.rank_test"
    require "test.rpc_test"
    require "test.protobuf_test"
    require "test.flatbuffers_test"
    require "test.timer_test"

    make_name()

    g_app.ok = true

    -- vd( statistic.dump() )
    t_run()

    ev:signal(2)
    ev:signal(15)

    ev:backend()
end

exec()
