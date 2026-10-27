-- 对C库json的封装，并提供其他常用接口
-- 底层的json库是https://github.com/openresty/lua-cjson
Json = {}

--[[
相对于官方https://github.com/mpx/lua-cjson，openresty增加了数组、object控制功能，例如：

local json = cjson.encode({
    foo = "bar",
    some_object = {},
    some_array = cjson.empty_array
})
===>>>
{
    "foo": "bar",
    "some_object": {},
    "some_array": []
}

更多的看官方文档即可

另一个备选库是https://github.com/xpol/lua-rapidjson，
支持int64并且也支持指定一个table编码为array或者object
]]

local cjson = require "cjson"

-- 文件默认都存runtime目录下
local BASE_DIR = "runtime" .. sys.separator

-- 把lua table编码为json字符串
-- 注意cjson和之前的lua_parson行为不一样：
-- 1. 数字key和字符串key混在一起的table会被编码为object，数字key会变成字符串
--    (如 {1, 2, unbind = 1} 编码为 {"1":1,"2":2,"unbind":1})
-- 2. 数组中的空洞会编码为null，过于稀疏的数组(下标超过10且密度低于1/2)会抛错
-- 3. json的null解码出来是Json.null(一个lightuserdata)，不是nil
Json.encode = cjson.encode

-- 把json字符串转换为lua table
Json.decode = cjson.decode

-- json中的null值，decode出来的null是它而不是nil，需要判空时和它比较
Json.null = cjson.null

-- 从指定json文件加载数据
-- @param name string 文件名，不包含runtime路径
function Json.load(name)
    local path = BASE_DIR .. name

    local f = io.open(path, "rb")
    if not f then
        return nil, "no such file"
    end

    local data = f:read("*a")
    f:close()

    if not data or "" == data then
        return nil, "unable to read from file"
    end

    return cjson.decode(data)
end

-- 从指定json文件加载数据
-- @param name string 文件名，不包含runtime路径
-- @param data table 要保存的数据
function Json.save(name, data)
    local path = BASE_DIR .. name

    local f = io.open(path, "rb")
    if not f then
        return nil, "unable to open file"
    end

    local str = cjson.encode(data)
    f:write(str)
    f:close()

    return true
end

-- 把lua table编码为json字符串后写入指定文件
-- cjson没有读写文件的接口，这里用io实现
-- @param tbl table 要编码的数据
-- @param path string 文件路径，包含完整路径
function Json.encode_to_file(tbl, path)
    local f = io.open(path, "wb")
    if not f then
        return nil, "unable to open file"
    end

    f:write(cjson.encode(tbl))
    f:close()

    return true
end

-- 从指定json文件读取内容并解码为lua table
-- @param path string 文件路径，包含完整路径
function Json.decode_from_file(path)
    local f = io.open(path, "rb")
    if not f then
        return nil, "no such file"
    end

    local data = f:read("*a")
    f:close()

    if not data or "" == data then
        return nil, "unable to read from file"
    end

    return cjson.decode(data)
end

return Json
