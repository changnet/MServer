-- code_performance.lua
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
