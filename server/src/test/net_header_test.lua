-- 业务协议头(NetHeader)测试
--
-- 重点是把「线上字节」钉死：端序一旦改错，两端都不会报错，只有数据错乱

local NetHeader = require "message.net_header"
local Buffer = require "engine.Buffer"

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
end)
