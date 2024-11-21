-- lua编码测试

local LuaCodec = require "engine.LuaCodec"

-- /////////////////////////////////////////////////////////////////////////////

t_describe("luacodec test", function()
    local codec = LuaCodec()

    local PERF_TIMES = 10000
    local params = {
        true,
        0x7FFFFFFFFFFFFFFF,
        "strrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrr",
        0xFFFFFFFFFFFFFFFF,
        false,
        9999999999.5555555,
        nil,
        1000,
        {
            true,
            0x7FFFFFFFFFFFFFFF,
            "strrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrr",
            0xFFFFFFFFFFFFFFFF,
            false,
            9999999999.5555555,
            nil,
            1000,
            {
                1, true, nil, "str", 9.009, false,
                {a = 1, [2] = 2, [false] = true, [9.5] = 9.5}
            }
        },
        {
            -- 使用基础类型作key
            [math.maxinteger] = math.maxinteger,
            [math.mininteger] = math.mininteger,
            str = "sssssssttttttttttttrrrrrrrrrrr",
            [1] = 1,
            [false] = true,
            [9.5] = 9.5,
            -- 使用table作key，支持但不推荐，
            -- 因为用内存地址作key用于判断是否相等是有问题的
            -- [{a = 1, [false] = true}] = { a = 999 },
        }
    }

    -- 这里的结构，尽量保持和pbc_codec_test中的一致，用于对比效率
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

    t_it("luacodec base test", function()
        -- 参数为空
        local buffer = codec:encode()
        local v1 = codec:decode(buffer)
        t_equal(v1, nil)

        -- 各种参数及table嵌套
        buffer = codec:encode(table.unpack(params))
        local decode_params = {codec:decode(buffer)}
        t_equal(decode_params, params)

        buffer = codec:encode(lite_pkt)
        v1 = codec:decode(buffer)
        t_equal(v1, lite_pkt)

        -- 超大内存，测试缓冲区重新分配
        local str = string.rep("0123456789", 6554)
        buffer = codec:encode(str)
        v1 = codec:decode(buffer)
        t_equal(v1, str)
    end)
    t_it(string.format("luacodec performance test %d", PERF_TIMES), function()
        local b1 = t_clock()
        for _ = 1, PERF_TIMES do
            codec:encode(lite_pkt)
        end
        local e1 = t_clock()

        local buffer = codec:encode(lite_pkt)

        local b2 = t_clock()
        for _ = 1, PERF_TIMES do
            codec:decode(buffer)
        end
        local e2 = t_clock()

        t_print(string.format("encode %d, decode = %d", e1 - b1, e2 - b2))
    end)
end)
