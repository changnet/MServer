require "global.global"
require "global.oo"
require "global.table"
require "global.string"
require "global.name"
require "statistic"
json = require "lua_parson"

g_timer_mgr = require "timer.timer_mgr"

local Application = require "application.application"

local App = oo.class( ...,Application )

-- 初始化
function App:__init( cmd, opts )
    Application.__init(self)
    self.cmd, self.name, self.filter = cmd, opts.app, opts.filter
end

function App:final_initialize()
    require "global.test"
    t_setup({
        print = PRINT,
        timer = {
            new = function(timeout, func)
                return g_timer_mgr:timeout(timeout/1000, func)
            end,

            del = function(timer_id)
                return g_timer_mgr:stop(timer_id)
            end
        },
        -- 过滤器，允许只执行一部分测试
        -- ./start.sh test --filter=https 只执行名字包含https的测试
        filter = self.filter,
        time_update = function()
            ev:time_update()
        end,
        clock = function() return ev:real_ms_time() end
    })

    require "test.misc_test"
    require "test.https_test"
    require "test.grid_aoi_test"
    require "test.list_aoi_test"
    -- require "example.mt_performance"
    require "test.mongodb_test"
    require "test.mysql_test"
    require "test.log_test"
    -- require "example.stream_performance"
    -- require "example.websocket_performance"
    require "test.words_filter_test"
    -- require "example.scene_performance"
    -- require "example.rank_performance"
    -- require "example.other_performance"

    -- vd( statistic.dump() )
    t_run()
end

return App

