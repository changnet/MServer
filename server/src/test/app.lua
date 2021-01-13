require "global.global"
require "global.oo"
require "global.table"
require "global.string"
require "global.name"
require "statistic"
json = require "lua_parson"

g_timer_mgr = require "timer.timer_mgr"

function sig_handler( signum )
    if g_mysql_mgr   then g_mysql_mgr:stop()   end
    if g_mongodb_mgr then g_mongodb_mgr:stop() end

    if g_log_mgr then g_log_mgr:stop(); end
    ev:exit()
end

local App = oo.class( ... )

-- 初始化
function App:__init( cmd, opts )
    self.cmd, self.name, self.filter = cmd, opts.app, opts.filter
end

-- 重写关服接口
function App:exec()
    ev:signal( 2 );
    ev:signal( 15 );

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
        -- ./start.sh test 1 1 https 只执行名字包含https的测试
        filter = self.filter,
        time_update = function()
            ev:time_update()
        end
    })

    require "test.misc_test"
    require "test.https_test"
    require "test.grid_aoi_test"
    require "test.list_aoi_test"
    -- require "example.mt_performance"
    -- require "example.mongo_performance"
    -- require "example.mysql_performance"
    -- require "example.log_performance"
    -- require "example.stream_performance"
    -- require "example.websocket_performance"
    -- require "example.words_filter_performance"
    -- require "example.scene_performance"
    -- require "example.rank_performance"
    -- require "example.other_performance"

    -- vd( statistic.dump() )
    t_run()
    ev:backend()
end

return App

