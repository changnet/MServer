-- misc_test.lua
-- xzc
-- 2016-03-06

local json = require "lua_parson"
local xml = require "lua_rapidxml"

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
    end)

    t_it("perf test", function()
        for i = 1, times do
            local tbl = json.decode(js)
            json.encode(tbl)
        end
    end)

    t_it("lua cjson dynamically loaded perf", function()
        local cjson = require "cjson"

        for i = 1, times do
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
    
    string.gsub(s, pt, function(w) table.insert(tbl, w) end)

    return tbl
end

function str_split3(str, delimiter, plain)
    local search_pos  = 1
    local sub_tab_len = 0
    local sub_str_tab = {}
    while true do
        local pos = string.find( str, delimiter,search_pos,plain )
        if not pos then
            local sub_str = string.sub( str, search_pos, -1 )
            table.insert( sub_str_tab,sub_tab_len + 1,sub_str )
            break
        end

        local sub_str = string.sub( str, search_pos, pos - 1 )
        table.insert( sub_str_tab,sub_tab_len + 1,sub_str )
        sub_tab_len = sub_tab_len + 1

        search_pos = pos + 1
    end

    return sub_str_tab
end

t_describe("string split performance test", function()
    local times = 100000
    local str1 = "aaaaaaaaaaaaa\tbbbbbbbbbbbbbbbbbbbbbbbb\tcccccccccccccc\t"
    local str2 = "aaaaaaaaaaaaaaaaaaaaaaaaabbbbbbbbbbbbbbbbbbbbbbcccccccccc"

    collectgarbage("stop")

    t_it("using string.find", function()
        local old_mem = collectgarbage("count")
        for i = 1, times do
            str_split3(str1, '\t')
            str_split3(str2, '\t')
        end

        t_print(string.format(
            "using memory %.03f kb", collectgarbage("count") - old_mem))
    end)

    t_it("using string.gmatch", function()
        local old_mem = collectgarbage("count")
        for i = 1, times do
            str_split(str1, '\t')
            str_split(str2, '\t')
        end

        t_print(string.format(
            "using memory %.03f kb", collectgarbage("count") - old_mem))
    end)

    t_it("using string.gsub", function()
        local old_mem = collectgarbage("count")
        for i = 1, times do
            str_split2(str1, '\t')
            str_split2(str2, '\t')
        end

        t_print(string.format(
            "using memory %.03f kb", collectgarbage("count") - old_mem))
    end)

    collectgarbage("restart")
end)

t_describe("table extend library test", function()
    t_it("table.sort_ex default compare func", function()
        local tbl = { 5,3,6,9,9,4,3,5,3 }
        table.sort_ex(tbl)

        t_equal(tbl, {3,3,3,4,5,5,6,9,9})
    end)

    t_it("table.sort_ex custom compare func", function()
        local tbl = { 5,3,6,9,9,4,3,5,3 }
        table.sort_ex(tbl, function(a, b)
            return a < b
        end)

        t_equal(tbl, {9,9,6,5,5,4,3,3,3})
    end)
end)


t_describe("util.head test", function()
    local MaxHeap = require "util.max_heap"

    local tbl = { 9,6,7,4,8,3,2,1,5 }

    t_it("max heap test", function()
        local expect = {}
        local mh = MaxHeap()
        for idx,val in pairs( tbl ) do
            mh:push(val,idx)
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
