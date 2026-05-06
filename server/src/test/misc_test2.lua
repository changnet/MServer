-- misc_test.lua
-- xzc
-- 2016-03-06

Test.describe("basen tests", function()
    local max_int64 = 0x7FFFFFFFFFFFFFFF
    local BaseN = require("idgen.basen")

    Test.it("basic test", function()
        Test.equal(BaseN.sequence62(0), "10")
        Test.equal(BaseN.sequence62(1), "11")
        Test.equal(BaseN.sequence62(61), "1z")
        Test.equal(BaseN.sequence62(max_int64), "BAzL8n0Y58m7")
        Test.equal(BaseN.sequence62(max_int64, max_int64), "BAzL8n0Y58m7BAzL8n0Y58m7")

        local new = BaseN.sequence62(3934340961738241457, 8738680431466283675)
        local old = BaseN.sequence62(5609499691562433253, 548312373745445620)

        Test.assert(old > new, "order error")

        Test.equal(BaseN.sequence36(0), "10")
        Test.equal(BaseN.sequence36(1), "11")
        Test.equal(BaseN.sequence36(35), "1z")
        Test.equal(BaseN.sequence36(35, 35), "1z1z")
    end)
    Test.it("fuzzy test", function()
        local last_hi
        local last_lo
        local last_s62
        for _ = 1, 10000 do
            local hi = math.random(1, max_int64)
            local lo = math.random(1, max_int64)
            local s62 = BaseN.sequence62(hi, lo)

            local ok = true
            local idx
            if last_hi then
                if hi > last_hi or (hi == last_hi and lo > last_lo) then
                    idx = 1
                    ok = (s62 > last_s62)
                elseif hi == last_hi and lo == last_lo then
                    idx = 2
                    ok = (s62 == last_s62)
                else
                    idx = 3
                    ok = (s62 < last_s62)
                end
            end
            if not ok then
                print("idx:", idx)
                print("new:", hi, lo, s62)
                print("old:", last_hi, last_lo, last_s62)
                Test.assert(false, "order error")
            end

            last_hi = hi
            last_lo = lo
            last_s62 = s62
        end
    end)
end)
