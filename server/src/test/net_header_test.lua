-- 业务协议头(NetHeader)测试
--
-- 重点是把「线上字节」钉死：端序一旦改错，两端都不会报错，只有数据错乱

local NetHeader = require "message.net_header"
local Buffer = require "engine.Buffer"
local EngineNetHeader = require "engine.NetHeader"

-- debug 构建才做 encode 范围校验（release 按低位截断）
local IS_DEBUG = g_sharedata and g_sharedata:get("__DEBUG__") == "true"

Test.describe("net header test", function()
    Test.it("header size", function()
        Test.equal(NetHeader.size("none"), 0)
        Test.equal(NetHeader.size("ss"), 16)
        Test.equal(NetHeader.size("ws"), 2)
    end)

    -- 小端是硬要求：老代码是结构体直接memcpy，线上就是x86小端。
    -- 这个用例是防「端序被改成大端」的唯一保险
    Test.it("encode golden bytes", function()
        local p, len = NetHeader.encode("ss", 1, 2, 3, 4)
        Test.equal(len, 16)
        Test.equal(Buffer.lightud_tostring(p, len),
                   string.char(1, 0, 0, 0, 2, 0, 0, 0, 3, 0, 0, 0, 4, 0, 0, 0))

        local wp, wlen = NetHeader.encode("ws", 0x1234)
        Test.equal(wlen, 2)
        Test.equal(Buffer.lightud_tostring(wp, wlen), string.char(0x34, 0x12))

    end)

    Test.it("encode decode roundtrip", function()
        local cases = {
            {0, 0, 0, 0},
            {1, 2, 3, 4},
            {-1, -1, -1, -1},
            {2147483647, -2147483648, 2147483647, -2147483648},
            {-123456789, 987654321, 0, 1}
        }

        local b = Buffer()

        for _, case in pairs(cases) do
            local p, len = NetHeader.encode("ss", table.unpack(case))
            -- 头后面接一段载荷，验证decode返回的指针正好指向头之后
            local frame = Buffer.lightud_tostring(p, len) .. "payload"
            local ud = b:fromstring(frame)

            local src, dst, mtype, usize, body, bsize =
                NetHeader.decode("ss", ud, #frame)

            Test.equal(src, case[1])
            Test.equal(dst, case[2])
            Test.equal(mtype, case[3])
            Test.equal(usize, case[4])
            Test.equal(bsize, 7)
            Test.equal(Buffer.lightud_tostring(body, bsize), "payload")
        end

    end)

    -- 无载荷时usize是自定义数据而不是长度，所以body必须给nil，
    -- 不能给一个"指向帧尾的合法指针"，否则调用方拿usize去读就越界了
    Test.it("decode empty payload", function()
        local p, len = NetHeader.encode("ss", 1, 2, -1, 999)
        local b = Buffer()
        local ud = b:fromstring(Buffer.lightud_tostring(p, len))

        local src, dst, mtype, usize, body, bsize =
            NetHeader.decode("ss", ud, len)

        Test.equal(src, 1)
        Test.equal(dst, 2)
        Test.equal(mtype, -1)
        Test.equal(usize, 999)
        Test.equal(body, nil)
        Test.equal(bsize, 0)

    end)

    -- 数据不足一个头时必须返回nil，由调用方关连接，不能猜
    Test.it("decode broken frame", function()
        local p, len = NetHeader.encode("ss", 1, 2, 3, 4)
        local b = Buffer()
        local ud = b:fromstring(Buffer.lightud_tostring(p, len - 1))

        Test.equal(NetHeader.decode("ss", ud, len - 1), nil)
        Test.equal(NetHeader.decode("ws", ud, 1), nil)

    end)

    -- 字段数不匹配会静默错位，必须报错
    Test.it("encode arg count", function()
        Test.assert(not pcall(NetHeader.encode, "ss", 1, 2, 3))
        Test.assert(not pcall(NetHeader.encode, "ss", 1, 2, 3, 4, 5))
        Test.assert(not pcall(NetHeader.encode, "none", 1))

    end)

    -- 描述符按 string.pack 语义：i/I 的宽度由数字后缀决定，
    -- 其它 token 的后缀必须等于固有宽度，否则就是下一个"静默错位"
    Test.it("descriptor suffix", function()
        local h = EngineNetHeader()

        Test.equal(h:size(h:define("i")), 4)
        Test.equal(h:size(h:define("i4")), 4)
        Test.equal(h:size(h:define("i8")), 8)
        Test.equal(h:size(h:define("i4i8")), 12)
        Test.equal(h:size(h:define("I1")), 1)
        Test.equal(h:size(h:define("H2")), 2)
        Test.equal(h:size(h:define("q8")), 8)
        Test.equal(h:size(h:define("q")), 8)
        Test.equal(h:size(h:define("<i2i2")), 4)

        -- H4 只占2字节、b2 只占1字节、i 的宽度越界，都必须报错
        Test.assert(not pcall(h.define, h, "H4"))
        Test.assert(not pcall(h.define, h, "b2"))
        Test.assert(not pcall(h.define, h, "i0"))
        Test.assert(not pcall(h.define, h, "i9"))
        Test.assert(not pcall(h.define, h, "iZ"))

        -- string.pack 里支持但我们没实现的，报错要说清是"不支持"
        Test.assert(not pcall(h.define, h, "s2"))
        Test.assert(not pcall(h.define, h, "d"))
    end)

    -- i8 真的占 8 字节（老实现会把 i8 当成 i4），顺便验证补码
    Test.it("i8 roundtrip", function()
        local h = EngineNetHeader()
        local id = h:define("i8")
        local b = Buffer()

        local p, len = h:encode(id, -1)
        Test.equal(len, 8)
        Test.equal(Buffer.lightud_tostring(p, len), string.rep("\255", 8))

        local ud = b:fromstring(Buffer.lightud_tostring(p, len))
        Test.equal(h:decode(id, ud, len), -1)

        local p2, len2 = h:encode(id, math.maxinteger)
        local ud2 = b:fromstring(Buffer.lightud_tostring(p2, len2))
        Test.equal(h:decode(id, ud2, len2), math.maxinteger)
    end)

    -- debug 下超范围直接报错（70000 进 H2 若静默截断会变成 4464）
    Test.it("encode range", function()
        local h = EngineNetHeader()
        local id = h:define("H2")

        if IS_DEBUG then
            Test.assert(not pcall(h.encode, h, id, 70000))
            Test.assert(not pcall(h.encode, h, id, -1))
            Test.assert(not pcall(h.encode, h, id, 65536))
        else
            local p, len = h:encode(id, 70000)
            Test.equal(Buffer.lightud_tostring(p, len), string.char(0x70, 0x11))
        end

        -- 边界值始终合法
        Test.assert(pcall(h.encode, h, id, 65535))
        Test.assert(pcall(h.encode, h, id, 0))

        -- 8 字节不校验：I8 传 -1 合法（同 string.pack）
        local id8 = h:define("I8")
        Test.assert(pcall(h.encode, h, id8, -1))
    end)

    -- decode 与 encode 对称：只收 lightuserdata，误传 string 必须报错，
    -- 不能退化成"帧太短 → 返回 nil → 上层关连接"这种莫名断线
    Test.it("decode needs lightuserdata", function()
        local h = EngineNetHeader()
        local id = h:define("H2")

        Test.assert(not pcall(h.decode, h, id, "ab", 2))
        Test.assert(not pcall(h.decode, h, id, nil, 2))
        Test.assert(not pcall(h.decode, h, id, 12, 2))
    end)
end)
