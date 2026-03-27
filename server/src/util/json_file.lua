-- 加载、保存数据为json文件，默认在runtime目录下
JsonFile = {}

local json = require "engine.lua_parson"

-- 文件默认都存runtime目录下
local BASE_DIR = "runtime" .. sys.separator

-- 从指定json文件加载数据
-- @param name string 文件名，不包含runtime路径
function JsonFile.load(name)
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

    return json.decode(data)
end

-- 从指定json文件加载数据
-- @param name string 文件名，不包含runtime路径
-- @param data table 要保存的数据
function JsonFile.save(name, data)
    local path = BASE_DIR .. name

    local f = io.open(path, "rb")
    if not f then
        return nil, "unable to open file"
    end

    local str = json.encode(data)
    f:write(str)
    f:close()

    return true
end
