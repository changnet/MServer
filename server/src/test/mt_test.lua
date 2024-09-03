-- lua metatable performance test
-- 2016-04-01
-- xzc
--[[
测试结果表明，使用元表速度更慢。因此这套框架的继承一般不要超过3层。
local ts = 10000000
call function native	1091772	microsecond
call function as table value	1287172	microsecond
call function with 3 level metatable	2014431	microsecond
call function with 10 level metatable	3707181	microsecond
[T0LP09-02 15:10:56]    [  OK] call function native 10000000 times (449ms)
[T0LP09-02 15:10:56]    [  OK] call function as table value 10000000 times (555ms)
[T0LP09-02 15:10:58]    [  OK] call function 1 level metatable 10000000 times (1110ms)
[T0LP09-02 15:11:00]    [  OK] call function 3 level metatable 10000000 times (2033ms)
[T0LP09-02 15:11:05]    [  OK] call function 10 level metatable 10000000 times (5015ms)

]]

t_describe("metatable test", function()
    local test = function(a, b)
        return a + b
    end

    local empty_mt1 = {}
    local empty_mt2 = {}
    local empty_mt3 = {}
    local empty_mt4 = {}
    local empty_mt5 = {}
    local empty_mt6 = {}
    local empty_mt7 = {}
    local empty_mt8 = {}

    local mt = {}
    mt.test = test

    local mt_tb = {}

    setmetatable(empty_mt8, {__index = mt})
    setmetatable(empty_mt7, {__index = empty_mt8})
    setmetatable(empty_mt6, {__index = empty_mt7})
    setmetatable(empty_mt5, {__index = empty_mt6})
    setmetatable(empty_mt4, {__index = empty_mt5})
    setmetatable(empty_mt3, {__index = empty_mt4})
    setmetatable(empty_mt2, {__index = empty_mt3})
    setmetatable(empty_mt1, {__index = empty_mt2})
    setmetatable(mt_tb, {__index = empty_mt1})


    local ts = 10000000

    t_it(string.format("call function native %d times", ts), function()
        local cnt = 0
        for _ = 1, ts do cnt = test(cnt, 1) end
    end)

    t_it(string.format("call function as table value %d times", ts), function()
        local cnt = 0
        for _ = 1, ts do cnt = mt.test(cnt, 1) end
    end)

    t_it(string.format("call function 1 level metatable %d times", ts),
        function()
            local cnt = 0
            for _ = 1, ts do cnt = empty_mt8.test(cnt, 1) end
        end
    )

    t_it(string.format("call function 3 level metatable %d times", ts),
        function()
            local cnt = 0
            for _ = 1, ts do cnt = empty_mt6.test(cnt, 1) end
        end
    )

    t_it(string.format("call function 10 level metatable %d times", ts),
        function()
            local cnt = 0
            for _ = 1, ts do cnt = mt_tb.test(cnt, 1) end
        end
    )
end)
