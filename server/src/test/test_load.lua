
require "global.table"
require "global.string"

require "global.test"

require "test.misc_test"
require "test.mt_test"

-- require "test.https_test"
-- require "test.websocket_test"
-- require "test.lua_codec_test"
-- require "test.pbc_codec_test"
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
    skip = g_env:get("skip"),
    -- 过滤器，允许只执行一部分测试
    -- ./start.sh test --filter=https 只执行名字包含https的测试
    filter = g_env:get("filter"),
    time_update = function()
        -- ev:time_update()
    end,
    clock = function()
        return g_engine:steady_clock()
    end
})

t_run()
