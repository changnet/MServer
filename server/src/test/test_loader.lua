
require "global.table"
require "global.string"

require "global.test"

require "test.misc_test"
require "test.heap_test"
require "test.mt_test"
require "test.timer_test"
require "test.https_test"
require "test.websocket_test"
require "test.lua_codec_test"
require "test.pbc_codec_test"
require "test.co_test"
require "test.socket_test"
require "test.log_test"
require "test.mysql_test"
require "test.mongodb_test"

--[[
require "test.grid_aoi_test"
require "test.list_aoi_test"
require "test.words_filter_test"
require "test.rank_test"
require "test.rpc_test"
require "test.flatbuffers_test"
]]

Test.setup({
    print = print,
    R = Log.redf,
    G = Log.greenf,
    B = Log.bluef,
    Y = Log.yellowf,
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
    skip = g_env:get("--skip"),
    -- 过滤器，允许只执行一部分测试
    -- ./start.sh test --filter=https 只执行名字包含https的测试
    filter = g_env:get("--filter"),
    time_update = function()
        Engine.update()
    end,
    clock = function()
        return Engine.steady_clock()
    end
})

Timer.timeout(0, function()
    -- 测试中有不少性能测试，避免gc影响测试结果
    -- 但即使设置了停止标识，也无法完全停止gc，断点luaC_step函数就可以看到
    collectgarbage("stop")

    Test.run()

    collectgarbage("restart")
end)

