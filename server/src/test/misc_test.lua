-- misc_test.lua
-- xzc
-- 2016-03-06
local json = require "lua_parson"
local xml = require "lua_rapidxml"
local util = require "util"

local js = [==[
{
    "name":"xzc",
    "chinese":"中文",
    "emoji":[ "\ue513","\u2600","\u2601","\u2614" ]
}
]==]

t_describe("json lib test", function()
    local times = 10000
    t_it("encode decode", function()
        local tbl = json.decode(js)
        local str = json.encode(tbl)
        local tbl_ex = json.decode(str)
        t_equal(tbl, tbl_ex)

        local cjson = require "cjson"
        local tbl_cj = cjson.decode(js)
        t_equal(tbl, tbl_cj)

        -- 测试table与json自动转换，尤其是整数key的转换
        tbl = {1, 2, unbind = 1, }
        str = json.encode(tbl, false, 8.6)
        tbl_ex = json.decode(str, false, 8.6)
        t_equal(tbl, tbl_ex)
    end)

    t_it("perf test", function()
        for _ = 1, times do
            local tbl = json.decode(js)
            json.encode(tbl)
        end
    end)

    t_it("lua cjson dynamically loaded perf", function()
        local cjson = require "cjson"

        for _ = 1, times do
            local tbl = cjson.decode(js)
            cjson.encode(tbl)
        end
    end)
end)

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

t_describe("xml lib test", function()
    t_it("encode decode", function()
        local tbl = xml.decode(xml_str)
        local str = xml.encode(tbl)
        local tbl_ex = xml.decode(str)
        t_equal(tbl, tbl_ex)
    end)
end)

t_describe("string extend library test", function()
    t_it("string.split", function()
        local list = string.split("a,b,c", ",")
        t_equal(list, {"a", "b", "c"})
    end)

    t_it("string.trim", function()
        local str = string.trim("   \tabc\t   ")
        t_equal(str, "abc")
    end)

    t_it("string.replace", function()
        local str = string.replace("abcabc", "abc", "11111")
        t_equal(str, "11111abc")
    end)

    t_it("string.replace_all", function()
        local str = string.replace_all("abcabc", "abc", "1111")
        t_equal(str, "11111111")
    end)

    t_it("string.replace_at greater", function()
        local str = string.replace_at("abcabc", "1111", 4)
        t_equal(str, "abc1111")
    end)

    t_it("string.replace_at less", function()
        local str = string.replace_at("abcabc", "1", 4)
        t_equal(str, "abc1bc")
    end)

    t_it("string.replace_at insert", function()
        local str = string.replace_at("abcabc", "111", 4, 4)
        t_equal(str, "abc111abc")
    end)

    t_it("string.gescape", function()
        local str = string.gescape("[.+*]%abcd")
        t_equal(str, "%[%.%+%*%]%%abcd")
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

t_describe("string split performance test", function()
    local times = 100000
    local str1 = "aaaaaaaaaaaaa\tbbbbbbbbbbbbbbbbbbbbbbbb\tcccccccccccccc\t"
    local str2 = "aaaaaaaaaaaaaaaaaaaaaaaaabbbbbbbbbbbbbbbbbbbbbbcccccccccc"

    t_before(function()
        collectgarbage("stop")
    end)

    t_it("using string.find", function()
        local old_mem = collectgarbage("count")
        for _ = 1, times do
            str_split3(str1, '\t')
            str_split3(str2, '\t')
        end

        t_print(string.format("using memory %.03f kb",
                              collectgarbage("count") - old_mem))
    end)

    t_it("using string.gmatch", function()
        local old_mem = collectgarbage("count")
        for _ = 1, times do
            str_split(str1, '\t')
            str_split(str2, '\t')
        end

        t_print(string.format("using memory %.03f kb",
                              collectgarbage("count") - old_mem))
    end)

    t_it("using string.gsub", function()
        local old_mem = collectgarbage("count")
        for _ = 1, times do
            str_split2(str1, '\t')
            str_split2(str2, '\t')
        end

        t_print(string.format("using memory %.03f kb",
                              collectgarbage("count") - old_mem))
    end)

    t_after(function()
        collectgarbage("restart")
    end)
end)

t_describe("table extend library test", function()
    t_it("table.sort_ex default compare func", function()
        local tbl = {5, 3, 6, 9, 9, 4, 3, 5, 3}
        table.sort_ex(tbl)

        t_equal(tbl, {3, 3, 3, 4, 5, 5, 6, 9, 9})
    end)

    t_it("table.sort_ex custom compare func", function()
        local tbl = {5, 3, 6, 9, 9, 4, 3, 5, 3}
        table.sort_ex(tbl, function(a, b)
            return a < b
        end)

        t_equal(tbl, {9, 9, 6, 5, 5, 4, 3, 3, 3})
    end)
end)

t_describe("util.head test", function()
    local MaxHeap = require "util.max_heap"

    local tbl = {9, 6, 7, 4, 8, 3, 2, 1, 5}

    t_it("max heap test", function()
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

            t_equal(obj.key, exp[1])
            t_equal(obj.value, exp[2])

            mh:pop()
            index = index + 1
        end
    end)

    -- TODO 重新实现heap，参考C++的priority_queue，根据传入的对比函数实现
    -- 大小堆
    t_it("min heap test", function()
        t_print("UNIMPLEMENTED !!")
    end)

    -- 参考boost，加一个id，对比id的大小实现稳定heap https://www.boost.org/doc/libs/develop/doc/html/heap.html
    t_it("heap stable", function()
        t_print("UNIMPLEMENTED !!")
    end)
end)

t_describe("crypto test", function()
    local str0 = "123456789"
    local str1 = "abcdefghi"
    local str = str0 .. str1

    t_it("md5", function()
        t_equal(util.md5(str), "60ff7843f0d81d2666e2f00b242dd467")
        t_equal(util.md5(true, str), "60FF7843F0D81D2666E2F00B242DD467")
        t_equal(util.md5(str), util.md5(str0, str1))
        t_equal(util.md5(true, str), util.md5(true, str0, str1))
    end)

    t_it("sha1", function()
        t_equal(util.sha1(str), "e34f10117b9d903e5c4c7e751c57db0426c33661")
        t_equal(util.sha1(true, str), "E34F10117B9D903E5C4C7E751C57DB0426C33661")
        t_equal(util.sha1(str), util.sha1(str0, str1))
        t_equal(util.sha1(true, str), util.sha1(true, str0, str1))
    end)

    t_it("base64", function()
        t_equal(util.base64(str), "MTIzNDU2Nzg5YWJjZGVmZ2hp")
    end)
end)

t_describe("table lib test", function()
    t_it("table.reverse", function()
        table.reverse({})
        table.reverse({}, 1, 100)
        table.reverse({}, 200, 100)

        t_equal(table.reverse({1}), {1})
        t_equal(table.reverse({1, 2, 3, 4, 5}), {5, 4, 3, 2, 1})
        t_equal(table.reverse({1, 2, 3, 4, 5}, 1, 3), {3, 2, 1, 4, 5})
    end)
end)

t_describe("math lib test", function()
    require("global.math")
    t_it("math from to base", function()
        -- 任意进制转换
        -- 注意这里没有反转，所成2进制的2是01而不是10
        t_equal(math.to_base(0, 2), "0")
        t_equal(math.from_base("0", 2), 0)
        t_equal(math.to_base(2, 2), "01")
        t_equal(math.from_base("01", 2), 2)

        t_equal(math.to_base(0, 36), "0")
        t_equal(math.from_base("0", 36), 0)
        t_equal(math.to_base(35, 36), "z")
        t_equal(math.from_base("z", 36), 35)

        for num = 1, 100 do
            local str = math.to_base(num, 36)
            t_equal(math.from_base(str, 36), num)
        end

        -- 进制转换可以使用外网工具来验证 https://tool.oschina.net/hexconvert/
        -- oschina那这个转换如果数字大于js能表示的大小，是不准的，用
        -- https://tool.lu/hexconvert/
        t_equal(math.to_base(8888888888, 36), "8kq7034")
        t_equal(math.from_base("8kq7034", 36), 8888888888)

        -- 这人数字超过double有表示的值，在lua中执行除法时，需要用 //
        local i = 7022597144829442788
        local s = math.to_base(i, 36)
        t_equal(s, "canca0bxarch1")
        t_equal(math.from_base(s, 36), i)

        i = math.maxinteger
        s = math.to_base(i, 36)
        t_equal(s, "7e8e23ji0p2y1")
        t_equal(math.from_base(s, 36), i)

        for _ = 1, 500 do
            local num = math.random(0, math.maxinteger)
            local str = math.to_base(num, 36)
            t_equal(math.from_base(str, 36), num)
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
            t_equal(math.from_base(str, base, digits), num)
        end

        i = math.maxinteger
        s = math.to_base(i, base, chars)
        t_equal(s, "zzzzzzzzzzzz9")
        t_equal(math.from_base(s, base, digits), i)
    end)
end)

t_describe("uri lib test", function()
    t_it("uri base test", function()
        local uri = require("util.uri")
        local url, fields = uri.parse("/web_gm?gm=@ghf")
        t_equal(url, "/web_gm")
        t_equal(fields, {gm = "@ghf"})

        -- 只有字段，没有值，也算合法的请求
        url, fields = uri.parse("/web_gm?gm")
        t_equal(url, "/web_gm")
        t_equal(fields, {gm = ""})

        url, fields = uri.parse("/www/web_gm?gm=1&srv=222")
        t_equal(url, "/www/web_gm")
        t_equal(fields, {gm = "1", srv = "222"})
    end)
end)
