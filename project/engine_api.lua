-- 导出引擎中的接口
-- 用于代码提示和luacheck

local entrance = "lstate.cpp"
local dir = "../engine/src/lua_cpplib/"

local set_regex = "^%s*lc%.set%(%w+::([_%w]+),%s*\"([_%w]+)\"%)"
local define_regex = "^%s*lc%.def<&%w+::([_%w]+)>%(\"([_%w]+)\"%)"
local class_regex = "^%s*[LBaseClass%|%LClass]+<([_%w]+)>.+%(.+,%s*\"([_%w]+)\"%)"

local api = {}
local last_class = nil

local function to_readable( val )
    if type(val) == "string" then
        return "\"" .. val .. "\""
    end

    return val
end

--- @param data 要打印的字符串
--- @param [max_level] table要展开打印的计数，默认nil表示全部展开
--- @param [prefix] 用于在递归时传递缩进，该参数不供用户使用于
local recursion = {}
local function var_dump(data, max_level, prefix)
    if type(prefix) ~= "string" then
        prefix = ""
    end

    if type(data) ~= "table" then
        print(prefix .. tostring(data))
    elseif recursion[data] then
        print(data,"dumplicate")  --重复递归
    else
        recursion[data] = true
        print(data)

        local prefix_next = prefix .. "    "
        print(prefix .. "{")
        for k,v in pairs(data) do
            io.stdout:write(prefix_next .. tostring( to_readable(k) ) .. " = ")
            if type(v) ~= "table" or
                (type(max_level) == "number" and max_level <= 1) then
                print( to_readable(v) )
            else
                var_dump(v, max_level - 1, prefix_next)
            end
        end
        print(prefix .. "}")
    end
end

--[[
    eg: local b = {aaa="aaa",bbb="bbb",ccc="ccc"}
]]
local function vd(data, max_level)
    var_dump(data, max_level or 20)
    recursion = {}  --释放内存
end

-- 解析一行代码
local function parse_line(line)
    local class, class_name = string.match(line, class_regex)
    if class then
        last_class =
        {
            class = class,
            class_name = class_name,

            set = {},
            define = {}
        }
        assert(not api[class_name])
        api[class_name] = last_class
        return
    end

    local define, define_name = string.match(line, define_regex)
    if define then
        assert(not last_class.define[define_name])
        last_class.define[define_name] =
        {
            define = define,
            define_name = define_name
        }
        return
    end

    local set, set_name = string.match(line, set_regex)
    if set then
        assert(not last_class.set[set_name])
        last_class.set[set_name] =
        {
            set = set,
            set_name = set_name
        }
        return
    end
end

-- 从lstate.cpp中解析要导出的api
local function parse_api()
    for line in io.lines(dir .. entrance) do
        parse_line(line)
    end
end

parse_api()
vd(api)
