require "global.global"
require "global.oo"
require "global.table"
require "global.string"
require "global.name"
require "statistic"
json = require "lua_parson"

g_timer_mgr = require "timer.timer_mgr"

local Application = require "application.application"

local App = oo.class(..., Application)

-- 初始化
function App:__init(cmd, opts)
    Application.__init(self)
    self.cmd, self.name, self.filter, self.skip = cmd, opts.app, opts.filter,
                                                  opts.skip
end

function App:final_initialize()
    require "global.test"
    t_setup({
        print = print,
        timer = {
            new = function(timeout, func)
                return g_timer_mgr:timeout(timeout, func)
            end,

            del = function(timer_id)
                return g_timer_mgr:stop(timer_id)
            end
        },
        -- 跳过这些测试
        -- ./start.sh test --skip='mongodb;sql'
        skip = self.skip,
        -- 过滤器，允许只执行一部分测试
        -- ./start.sh test --filter=https 只执行名字包含https的测试
        filter = self.filter,
        time_update = function()
            ev:time_update()
        end,
        clock = function()
            return ev:real_ms_time()
        end
    })

    -- 随机一个session，部分功能用到，如测试协议派发时
    self.session = 0x10001
    network_mgr:set_curr_session(self.session)

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
    -- require "example.scene_performance"
    require "test.rank_test"
    require "test.rpc_test"
    require "test.protobuf_test"
    -- require "example.other_performance"

    make_name()

    -- vd( statistic.dump() )
    t_run()
end

return App

