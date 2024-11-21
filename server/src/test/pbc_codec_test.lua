-- lua编码测试

local PbcCodec = require "engine.PbcCodec"

-- /////////////////////////////////////////////////////////////////////////////

t_describe("pbccodec test", function()
    local codec = PbcCodec()

    codec:reset()
    for _, fpath in pairs({"../pb/system.pb"}) do
        local f = io.open(fpath, "rb") -- 必须以binary模式打开
        local buffer = f:read("a")
        f:close()

        local ok = codec:load(buffer)
        assert(ok)
    end
    codec:update()

    local PERF_TIMES = 10000
    local rep_cnt = 512 -- 512的时候，整个包达到50000多字节了
    local base_pkt = {
        d1 = -99999999999999.55555,
        d2 = 99999999999999.55555,
        -- float的精度超级差，基本在0.5，过不了t_equal
        -- 如11111111.5678就会变成11111112，在C++中也一样
        -- f1 = -111.6789,
        -- f2 = 111.1234,
        i1 = -2147483648;
        i2 = 2147483647;
        i641 = math.mininteger,
        i642 = math.maxinteger,
        b1 = true,
        -- b2 = false, -- pbc不传输默认值，加了这个会导致t_equal检测失败
        s1 = "s", -- 空字符串是默认值 ，pbc也不打包的
        s2 = string.rep("ssssssssss", rep_cnt),
        by1 = "s"; -- 空字符串是默认值 ，pbc也不打包的
        by2 = string.rep("ssssssssss", rep_cnt);
        ui1 = 1,
        ui2 = 4294967295,
        ui641 = 1,
        ui642 = math.maxinteger, -- 0xffffffffffffffff, lua不支持uint64_t
        -- fix相当于 uint32, sint才是有符号的
        f321 = 2147483648,
        f322 = 2147483647,
        f641 = 1,
        f642 = math.maxinteger, -- 0xffffffffffffffff, lua不支持uint64_t
    }
    local cpy = table.copy(base_pkt)
    base_pkt.msg1 = cpy
    base_pkt.i_list = {1,2,3,4,5,99999,55555,111111111}
    base_pkt.msg_list = { cpy, cpy, cpy}

    -- 这里的结构，尽量保持和lua_codec_test中的一致，用于对比效率
    local lite_pkt = {
        b1 = true,
        i1 = math.maxinteger,
        s = "sssssssssssssssssssssssssssssssssssssssssssssssssss",
        i2 = math.mininteger,
        d1 = 99999999999999.55555,
        i3 = 2147483647,

        msg = {
            b1 = true,
            i1 = math.maxinteger,
            s = "sssssssssssssssssssssssssssssssssssssssssssssssssss",
            i2 = math.mininteger,
            d1 = 99999999999999.55555,
            i3 = 2147483647,
        }
    }

    t_it("pbccodec base test", function()
        local buffer = codec:encode("system.TestBase", base_pkt)
        local v1 = codec:decode("system.TestBase", buffer)
        t_equal(v1, base_pkt)

        buffer = codec:encode("system.TestLite", lite_pkt)
        v1 = codec:decode("system.TestLite", buffer)
        t_equal(v1, lite_pkt)
    end)
    t_it(string.format("pbccodec performance test %d", PERF_TIMES), function()
        local b1 = t_clock()
        for _ = 1, PERF_TIMES do
            codec:encode("system.TestLite", lite_pkt)
        end
        local e1 = t_clock()

        local buffer = codec:encode("system.TestLite", lite_pkt)

        local b2 = t_clock()
        for _ = 1, PERF_TIMES do
            codec:decode("system.TestLite", buffer)
        end
        local e2 = t_clock()

        t_print(string.format("encode %d, decode = %d", e1 - b1, e2 - b2))
    end)
end)
