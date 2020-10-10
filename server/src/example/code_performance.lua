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
    t_it("encode decode", function()
        local tbl = json.decode(js)
        local str = json.encode(tbl)
        local tbl_ex = json.decode(str)
        t_equal(tbl, tbl_ex)
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

t_describe("c module(so or dll) dynamically loaded test", function()
    t_it("big integer library test", function()
        local lua_bigint = require "lua_bigint"
        t_equal(tostring(lua_bigint(1000000001)), "1000000001")
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
