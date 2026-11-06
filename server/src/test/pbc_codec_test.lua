-- lua编码测试

local PbcCodec = require "engine.PbcCodec"

-- /////////////////////////////////////////////////////////////////////////////

Test.describe("pbccodec test", function()
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

    -- proto3 map：kv table 直接编解码，覆盖标量/bool/string 键、message 值、默认值 entry
    local map_pkt = {
        id = 42,
        attrs = { [1] = 100, [2] = 200 },
        extras = { hp = "999", mp = "50" },
        flags = { [true] = 1, [false] = 0 },
        items = { sword = { id = 7, name = "blade" } },
        zeros = { [0] = 0 },
    }

    local function check_map(out)
        return out and out.id == 42
            and out.attrs and out.attrs[1] == 100 and out.attrs[2] == 200
            and out.extras and out.extras.hp == "999" and out.extras.mp == "50"
            and out.flags and out.flags[true] == 1 and out.flags[false] == 0
            and out.items and out.items.sword and out.items.sword.id == 7
            and out.items.sword.name == "blade"
            and out.zeros and out.zeros[0] == 0
    end

    Test.it("pbccodec base test", function()
        local buffer = codec:encode("system.TestBase", base_pkt)
        local v1 = codec:decode("system.TestBase", buffer)
        Test.equal(v1, base_pkt)

        local size
        buffer, size = codec:encode_to_buffer("system.TestBase", base_pkt)

        -- encode_to_buffer是一个依赖codec的指针，decode时会释放
        -- 所以临时存到Buffer中
        local Buffer = require "engine.Buffer"
        local b = Buffer()
        b:set(buffer, size)
        local new_buffer, new_size = b:get()

        v1 = codec:decode_from_buffer("system.TestBase", new_buffer, new_size)
        Test.equal(v1, base_pkt)

        buffer = codec:encode("system.TestLite", lite_pkt)
        v1 = codec:decode("system.TestLite", buffer)
        Test.equal(v1, lite_pkt)
    end)
    Test.it("pbccodec map test", function()
        -- 反射糖：区分 map 字段与普通字段
        Test.assert(codec:is_map("system.TestMap", "attrs"))
        Test.assert(not codec:is_map("system.TestMap", "id"))

        local buffer = codec:encode("system.TestMap", map_pkt)
        local out = codec:decode("system.TestMap", buffer)
        Test.assert(check_map(out), "map kv roundtrip")
        Test.equal(out, map_pkt)

        -- 业务实际走的是 buffer 指针路径，map 也必须一致
        local ptr, size = codec:encode_to_buffer("system.TestMap", map_pkt)
        local Buffer = require "engine.Buffer"
        local b = Buffer()
        b:set(ptr, size)
        local new_ptr, new_size = b:get()
        local from_buf = codec:decode_from_buffer("system.TestMap", new_ptr, new_size)
        Test.assert(check_map(from_buf), "map via buffer")
        Test.equal(from_buf, map_pkt)
    end)

    Test.it("pbccodec map default entry test", function()
        -- 空 map 不产生 entry，{[0]=0} 必须产生一条 entry，wire 与语义都不同
        local empty = codec:encode("system.TestMap", { id = 1, zeros = {} })
        local zero = codec:encode("system.TestMap", { id = 1, zeros = { [0] = 0 } })
        Test.assert(empty ~= zero)

        local out_empty = codec:decode("system.TestMap", empty)
        local out_zero = codec:decode("system.TestMap", zero)
        Test.assert(not out_empty.zeros or out_empty.zeros[0] == nil)
        Test.assert(out_zero.zeros and out_zero.zeros[0] == 0)
    end)

    Test.it(string.format("pbccodec performance test %d", PERF_TIMES), function()
        local b1 = Test.clock()
        for _ = 1, PERF_TIMES do
            codec:encode("system.TestLite", lite_pkt)
        end
        local e1 = Test.clock()

        local buffer = codec:encode("system.TestLite", lite_pkt)

        local b2 = Test.clock()
        for _ = 1, PERF_TIMES do
            codec:decode("system.TestLite", buffer)
        end
        local e2 = Test.clock()

        Test.print(string.format("encode %d, decode = %d", e1 - b1, e2 - b2))
    end)
end)
