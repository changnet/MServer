-- misc_test.lua
-- xzc
-- 2016-03-06
local json = require "engine.lua_parson"
local xml = require "engine.lua_rapidxml"
local util = require "engine.util"

local js = [==[
{
    "name":"xzc",
    "chinese":"中文",
    "emoji":[ "\ue513","\u2600","\u2601","\u2614" ]
}
]==]

Test.describe("json lib test", function()
    local times = 10000
    Test.it("encode decode", function()
        local tbl = json.decode(js)
        local str = json.encode(tbl)
        local tbl_ex = json.decode(str)
        Test.equal(tbl, tbl_ex)

        local cjson = require "cjson"
        local tbl_cj = cjson.decode(js)
        Test.equal(tbl, tbl_cj)

        -- 测试table与json自动转换，尤其是整数key的转换
        tbl = {1, 2, unbind = 1, }
        str = json.encode(tbl, false, 8.6)
        tbl_ex = json.decode(str, false, 8.6)
        Test.equal(tbl, tbl_ex)
    end)

    Test.it("perf test", function()
        for _ = 1, times do
            local tbl = json.decode(js)
            json.encode(tbl)
        end
    end)

    Test.it("lua cjson dynamically loaded perf", function()
        local cjson = require "cjson"

        for _ = 1, times do
            local tbl = cjson.decode(js)
            cjson.encode(tbl)
        end
    end)
end)

-- luacheck: push ignore
local xml_str = [==[
<?xml version="1.0" encoding="UTF-8"?>
<root>
   <base id="test001">
      <string>hello world</string>
      <number>44.95</number>
      <date>2000-10-01</date>
      <linebreak>An in-depth look at creating applications
      with XML.</linebreak>
    </base>
</root>
]==]
-- luacheck: pop

Test.describe("xml lib test", function()
    Test.it("encode decode", function()
        local tbl = xml.decode(xml_str)
        local str = xml.encode(tbl)
        local tbl_ex = xml.decode(str)
        Test.equal(tbl, tbl_ex)
    end)
end)

Test.describe("string extend library test", function()
    Test.it("string.split", function()
        local list = string.split("a,b,c", ",")
        Test.equal(list, {"a", "b", "c"})
    end)

    Test.it("string.trim", function()
        local str = string.trim("   \tabc\t   ")
        Test.equal(str, "abc")
    end)

    Test.it("string.replace", function()
        local str = string.replace("abcabc", "abc", "11111")
        Test.equal(str, "11111abc")
    end)

    Test.it("string.replace_all", function()
        local str = string.replace_all("abcabc", "abc", "1111")
        Test.equal(str, "11111111")
    end)

    Test.it("string.replace_at greater", function()
        local str = string.replace_at("abcabc", "1111", 4)
        Test.equal(str, "abc1111")
    end)

    Test.it("string.replace_at less", function()
        local str = string.replace_at("abcabc", "1", 4)
        Test.equal(str, "abc1bc")
    end)

    Test.it("string.replace_at insert", function()
        local str = string.replace_at("abcabc", "111", 4, 4)
        Test.equal(str, "abc111abc")
    end)

    Test.it("string.gescape", function()
        local str = string.gescape("[.+*]%abcd")
        Test.equal(str, "%[%.%+%*%]%%abcd")
    end)
end)

local function str_split(s, p)
    local tbl = {}
    local pt = '[^' .. p .. ']+'
    for w in string.gmatch(s, pt) do table.insert(tbl, w) end

    return tbl
end

local function str_split2(s, p)
    local tbl = {}
    local pt = '[^' .. p .. ']+'

    string.gsub(s, pt, function(w)
        table.insert(tbl, w)
    end)

    return tbl
end

function str_split3(str, delimiter, plain)
    local search_pos = 1
    local sub_tab_len = 0
    local sub_str_tab = {}
    while true do
        local pos = string.find(str, delimiter, search_pos, plain)
        if not pos then
            local sub_str = string.sub(str, search_pos, -1)
            table.insert(sub_str_tab, sub_tab_len + 1, sub_str)
            break
        end

        local sub_str = string.sub(str, search_pos, pos - 1)
        table.insert(sub_str_tab, sub_tab_len + 1, sub_str)
        sub_tab_len = sub_tab_len + 1

        search_pos = pos + 1
    end

    return sub_str_tab
end

Test.describe("string split performance test", function()
    local times = 100000
    local str1 = "aaaaaaaaaaaaa\tbbbbbbbbbbbbbbbbbbbbbbbb\tcccccccccccccc\t"
    local str2 = "aaaaaaaaaaaaaaaaaaaaaaaaabbbbbbbbbbbbbbbbbbbbbbcccccccccc"

    Test.before(function()
        collectgarbage("stop")
    end)

    Test.it("using string.find", function()
        local old_mem = collectgarbage("count")
        for _ = 1, times do
            str_split3(str1, '\t')
            str_split3(str2, '\t')
        end

        Test.print(string.format("using memory %.03f kb",
                              collectgarbage("count") - old_mem))
    end)

    Test.it("using string.gmatch", function()
        local old_mem = collectgarbage("count")
        for _ = 1, times do
            str_split(str1, '\t')
            str_split(str2, '\t')
        end

        Test.print(string.format("using memory %.03f kb",
                              collectgarbage("count") - old_mem))
    end)

    Test.it("using string.gsub", function()
        local old_mem = collectgarbage("count")
        for _ = 1, times do
            str_split2(str1, '\t')
            str_split2(str2, '\t')
        end

        Test.print(string.format("using memory %.03f kb",
                              collectgarbage("count") - old_mem))
    end)

    Test.after(function()
        collectgarbage("restart")
    end)
end)

Test.describe("table extend library test", function()
    Test.it("table.sort_ex default compare func", function()
        local tbl = {5, 3, 6, 9, 9, 4, 3, 5, 3}
        table.sort_ex(tbl)

        Test.equal(tbl, {3, 3, 3, 4, 5, 5, 6, 9, 9})
    end)

    Test.it("table.sort_ex custom compare func", function()
        local tbl = {5, 3, 6, 9, 9, 4, 3, 5, 3}
        table.sort_ex(tbl, function(a, b)
            return a < b
        end)

        Test.equal(tbl, {9, 9, 6, 5, 5, 4, 3, 3, 3})
    end)
end)

Test.describe("util.head test", function()
    Test.it("max heap test", function()
        local MaxHeap = require "util.max_heap"

        local tbl = {9, 6, 7, 4, 8, 3, 2, 1, 5}
        local expect = {}
        local mh = MaxHeap()
        for idx, val in pairs(tbl) do
            mh:push(val, idx)
            table.insert(expect, {val, idx})
        end

        -- stable sort
        table.sort_ex(expect, function(a, b)
            if a[1] ~= b[1] then return a[1] < b[1] end

            return a[2] > b[2]
        end)

        local index = 1
        while not mh:empty() do
            local obj = mh:top()
            local exp = expect[index]

            Test.equal(obj.key, exp[1])
            Test.equal(obj.value, exp[2])

            mh:pop()
            index = index + 1
        end
    end)

    -- TODO 重新实现heap，参考C++的priority_queue，根据传入的对比函数实现
    -- 大小堆
    Test.it("min heap test", function()
        Test.print("UNIMPLEMENTED !!")
    end)

    -- 参考boost，加一个id，对比id的大小实现稳定heap https://www.boost.org/doc/libs/develop/doc/html/heap.html
    Test.it("heap stable", function()
        Test.print("UNIMPLEMENTED !!")
    end)
end)

Test.describe("crypto test", function()
    local str0 = "123456789"
    local str1 = "abcdefghi"
    local str = str0 .. str1

    Test.it("md5", function()
        Test.equal(util.md5(str), "60ff7843f0d81d2666e2f00b242dd467")
        Test.equal(util.md5(true, str), "60FF7843F0D81D2666E2F00B242DD467")
        Test.equal(util.md5(str), util.md5(str0, str1))
        Test.equal(util.md5(true, str), util.md5(true, str0, str1))
    end)

    Test.it("sha1", function()
        Test.equal(util.sha1(str), "e34f10117b9d903e5c4c7e751c57db0426c33661")
        Test.equal(util.sha1(true, str), "E34F10117B9D903E5C4C7E751C57DB0426C33661")
        Test.equal(util.sha1(str), util.sha1(str0, str1))
        Test.equal(util.sha1(true, str), util.sha1(true, str0, str1))
    end)

    Test.it("base64", function()
        Test.equal(util.base64(str), "MTIzNDU2Nzg5YWJjZGVmZ2hp")
    end)
end)

Test.describe("table lib test", function()
    Test.it("table.reverse", function()
        table.reverse({})
        table.reverse({}, 1, 100)
        table.reverse({}, 200, 100)

        Test.equal(table.reverse({1}), {1})
        Test.equal(table.reverse({1, 2, 3, 4, 5}), {5, 4, 3, 2, 1})
        Test.equal(table.reverse({1, 2, 3, 4, 5}, 1, 3), {3, 2, 1, 4, 5})
    end)
end)

Test.describe("math lib test", function()
    require("global.math")
    Test.it("math from to base", function()
        -- 任意进制转换
        -- 注意这里没有反转，所成2进制的2是01而不是10
        Test.equal(math.to_base(0, 2), "0")
        Test.equal(math.from_base("0", 2), 0)
        Test.equal(math.to_base(2, 2), "01")
        Test.equal(math.from_base("01", 2), 2)

        Test.equal(math.to_base(0, 36), "0")
        Test.equal(math.from_base("0", 36), 0)
        Test.equal(math.to_base(35, 36), "z")
        Test.equal(math.from_base("z", 36), 35)

        for num = 1, 100 do
            local str = math.to_base(num, 36)
            Test.equal(math.from_base(str, 36), num)
        end

        -- 进制转换可以使用外网工具来验证 https://tool.oschina.net/hexconvert/
        -- oschina那这个转换如果数字大于js能表示的大小，是不准的，用
        -- https://tool.lu/hexconvert/
        Test.equal(math.to_base(8888888888, 36), "8kq7034")
        Test.equal(math.from_base("8kq7034", 36), 8888888888)

        -- 这人数字超过double有表示的值，在lua中执行除法时，需要用 //
        local i = 7022597144829442788
        local s = math.to_base(i, 36)
        Test.equal(s, "canca0bxarch1")
        Test.equal(math.from_base(s, 36), i)

        i = math.maxinteger
        s = math.to_base(i, 36)
        Test.equal(s, "7e8e23ji0p2y1")
        Test.equal(math.from_base(s, 36), i)

        for _ = 1, 500 do
            local num = math.random(0, math.maxinteger)
            local str = math.to_base(num, 36)
            Test.equal(math.from_base(str, 36), num)
        end

        -- 这是很经典的应用，去掉0、o、1、l等容易写错的字符，然后生成字符串作为礼品码
        -- 必要时，还可以把这些字符打乱，这样生成的字符串对别人来说就是没规律的
        local chars = {
            "2","3","4","5","6","7","8","9",
            "a","b","c","d","e","f","g","h","i","j",
            "k","m","n","p","q","r","s","t",
            "u","v","w","x","y","z"
        }
        local digits = {}
        for d, c in pairs(chars) do digits[c] = d - 1 end

        local base = #chars
        for num = 100000, 101000 do
            local str = math.to_base(num, base, chars)
            Test.equal(math.from_base(str, base, digits), num)
        end

        i = math.maxinteger
        s = math.to_base(i, base, chars)
        Test.equal(s, "zzzzzzzzzzzz9")
        Test.equal(math.from_base(s, base, digits), i)
    end)
end)

Test.describe("uri lib test", function()
    Test.it("uri base test", function()
        local uri = require("util.uri")
        local url, fields = uri.parse("/web_gm?gm=@ghf")
        Test.equal(url, "/web_gm")
        Test.equal(fields, {gm = "@ghf"})

        -- 只有字段，没有值，也算合法的请求
        url, fields = uri.parse("/web_gm?gm")
        Test.equal(url, "/web_gm")
        Test.equal(fields, {gm = ""})

        url, fields = uri.parse("/www/web_gm?gm=1&srv=222")
        Test.equal(url, "/www/web_gm")
        Test.equal(fields, {gm = "1", srv = "222"})
    end)
end)

-- 测试在lua中使用rawget和rawset效率到底有多慢（一般慢10倍）
Test.describe("rawset rawget test", function()
    Test.it("rawget rawset test", function()
        local TS = 1000000

        local tbl = {
            abc = 998
        }

        local b1 = Test.clock()
        for _ = 1, TS do
            local _ = tbl.abc
        end
        local e1 = Test.clock()

        local b2 = Test.clock()
        for _ = 1, TS do
            local _ = rawget(tbl, "abc")
        end
        local e2 = Test.clock()

        local b3 = Test.clock()
        for _ = 1, TS do
            tbl.abc = 996
        end
        local e3 = Test.clock()

        local b4 = Test.clock()
        for _ = 1, TS do
            local _ = rawset(tbl, "abc", 996)
        end
        local e4 = Test.clock()

        Test.print(string.format(
            "table operator[] %d, rawget %d, operator= %d, rawset %d",
            e1 - b1, e2 - b2, e3 - b3, e4 - b4
        ))
    end)
end)

-- 测试在lua中使用rawget和rawset效率到底有多慢（一般慢10倍）
Test.describe("rawset rawget test", function()
    Test.it("rawget rawset test", function()
        local TS = 1000000

        local tbl = {
            abc = 998
        }

        local b1 = Test.clock()
        for _ = 1, TS do
            local _ = tbl.abc
        end
        local e1 = Test.clock()

        local b2 = Test.clock()
        for _ = 1, TS do
            local _ = rawget(tbl, "abc")
        end
        local e2 = Test.clock()

        local b3 = Test.clock()
        for _ = 1, TS do
            tbl.abc = 996
        end
        local e3 = Test.clock()

        local b4 = Test.clock()
        for _ = 1, TS do
            local _ = rawset(tbl, "abc", 996)
        end
        local e4 = Test.clock()

        Test.print(string.format(
            "table operator[] %d, rawget %d, operator= %d, rawset %d",
            e1 - b1, e2 - b2, e3 - b3, e4 - b4
        ))
    end)
end)

-- 测试modi table和原生table相比效率有多慢（一般慢10倍）
Test.describe("modi table test", function()
    require "global.modi_table"
    Test.it("modi table base test", function()
        local expect = {
            i = 123456789,
            s = "abcdefghijk",
            t = {
                t = {
                    i = 123456789,
                    s = "123456789"
                },
                i = "34567",
                s = "789"
            }
        }
        local tbl = table.new_modi()
        Test.equal(tbl.modify, false)

        -- 赋值后是否正确
        for k, v in pairs(expect) do
            tbl[k] = v
        end
        Test.equal(tbl.data, expect)
        Test.equal(tbl.modify, true)

        -- 手动赋值后是否正确
        local tbl1 = table.new_modi()
        tbl1.i = expect.i
        tbl1.s = expect.s
        Test.equal(tbl1.modify, true)

        -- 单个table并且包含子table情况下是否正确
        tbl1.modify = false
        tbl1.t = expect.t
        Test.equal(tbl1.modify, true)

        -- 子table赋值取值是否正确
        tbl1.modify = false
        tbl1.t.t.i = 99878
        Test.equal(tbl1.modify, true)
        Test.equal(tbl1.t.t.i, 99878)

        -- 多次修改后，data是否完整
        tbl1.modify = false
        tbl1.t.t.i = expect.t.t.i
        Test.equal(tbl1.modify, true)
        Test.equal(tbl1.data, expect)

        -- 子table一层一层赋值，是否正确
        local tbl2 = table.new_modi()
        tbl2.t = {}
        Test.equal(tbl2.modify, true)

        tbl2.modify = false
        tbl2.t.t = {}
        Test.equal(tbl2.modify, true)

        tbl2.modify = false
        tbl2.t.t.i = 123456
        Test.equal(tbl2.modify, true)
        Test.equal(tbl2.t.t.i, 123456)

        tbl2.modify = false
        tbl2.t.t.i = 345
        Test.equal(tbl2.modify, true)
        Test.equal(tbl2.t.t.i, 345)
        Test.equal(tbl2.data.t.t.i, 345)

        -- 直接使用一个table来初始化
        local tbl3 = table.new_modi(expect)
        Test.equal(tbl3.modify, false)
        Test.equal(tbl3.t.t.i, expect.t.t.i)

        -- 这个应该会修改到原来的expect
        tbl3.t.t.i = 123
        Test.equal(tbl3.modify, true)
        Test.equal(tbl3.t.t.i, expect.t.t.i)
    end)
    Test.it("modi table level 1 test", function()
        local raw = {i = 1234567}
        local modi = table.new_modi()
        modi.i = 1234567

        local TS = 1000000

        local b1 = Test.clock()
        for _ = 1, TS do
            local _ = raw.i
        end
        local e1 = Test.clock()

        local b2 = Test.clock()
        for _ = 1, TS do
            local _ = modi.i
        end
        local e2 = Test.clock()

        local b3 = Test.clock()
        for _ = 1, TS do
            raw.i = 123
        end
        local e3 = Test.clock()

        local b4 = Test.clock()
        for _ = 1, TS do
            modi.i = 123
        end
        local e4 = Test.clock()

        Test.print(string.format(
            "modi raw get %d, modi get %d, raw set %d, modi set = %d",
            e1 - b1, e2 - b2, e3 - b3, e4 - b4
        ))
    end)
    Test.it("modi table level 2 test", function()
        local raw = {t = {i = 1234567}}
        local modi = table.new_modi()
        modi.t = {}
        modi.t.i = 1234567

        local TS = 1000000

        local b1 = Test.clock()
        for _ = 1, TS do
            local _ = raw.t.i
        end
        local e1 = Test.clock()

        local b2 = Test.clock()
        for _ = 1, TS do
            local _ = modi.t.i
        end
        local e2 = Test.clock()

        local b3 = Test.clock()
        for _ = 1, TS do
            raw.t.i = 123
        end
        local e3 = Test.clock()

        local b4 = Test.clock()
        for _ = 1, TS do
            modi.t.i = 123
        end
        local e4 = Test.clock()

        Test.print(string.format(
            "modi raw get %d, modi get %d, raw set %d, modi set = %d",
            e1 - b1, e2 - b2, e3 - b3, e4 - b4
        ))
    end)
    Test.it("modi table level 3 test", function()
        local raw = {t = {t = {i = 1234567}}}
        local modi = table.new_modi()
        modi.t = {}
        modi.t.t = {}
        modi.t.t.i = 1234567

        local TS = 1000000

        local b1 = Test.clock()
        for _ = 1, TS do
            local _ = raw.t.t.i
        end
        local e1 = Test.clock()

        local b2 = Test.clock()
        for _ = 1, TS do
            local _ = modi.t.t.i
        end
        local e2 = Test.clock()

        local b3 = Test.clock()
        for _ = 1, TS do
            raw.t.t.i = 123
        end
        local e3 = Test.clock()

        local b4 = Test.clock()
        for _ = 1, TS do
            modi.t.t.i = 123
        end
        local e4 = Test.clock()

        Test.print(string.format(
            "modi raw get %d, modi get %d, raw set %d, modi set = %d",
            e1 - b1, e2 - b2, e3 - b3, e4 - b4
        ))
    end)
end)

Test.describe("lua oo test", function()
    Test.it("lua oo test", function()
        local C1 = oo.class()
        function C1:test1()
            return 444
        end

        local C2 = oo.class(C1)
        function C2:test2()
            return 888
        end

        local c1 = C1()
        local c2 = C2()

        c2:test2()
        Test.assert(not c1.test2)
        Test.equal(c1:test1(), c2:test1())
        print(c1, c2)
    end)
end)
