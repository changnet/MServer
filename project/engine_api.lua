-- 导出引擎中的接口
-- 用于代码提示和luacheck
--[[
需要导出的注释，必须按doxygen格式，如下
/**
 * 这是多行注释
 */
int32_t test(luaState *L);

CONST = 1; ///< 这是单行注释
]]

-- 入口文件，所有导出的符号都是从这里开始查找的
local entrance = "lstate.cpp"

-- 这个目录下的所有文件都会被遍历，查找导出的符号
local dir = "../engine/src/lua_cpplib/"

-- 导出目录
local export_dir = "../server/engine/"

-- 部分符号在其他目录，这些文件也需要查找
local other_file =
{
    "../engine/src/net/socket.h",
    "../engine/src/net/io/io.h",
    "../engine/src/net/codec/codec.h",
}

local set_regex = "^%s*lc%.set%(%w+::([_%w]+),%s*\"([_%w]+)\"%)"
local define_regex = "^%s*lc%.def<&%w+::([_%w]+)>%(\"([_%w]+)\"%)"
local lib_regex = "^%s*LUA_LIB_OPEN%(\"([_%w]+)\"%s*,%s*([_%w]+)%)"
local class_regex = "^%s*[LBaseClass%|%LClass]+<([_%w]+)>.+%(.+,%s*\"([_%w]+)\"%)"

local libs = {}
local lib_file = {}
local classes = {}
local comments = {}

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
function vd(data, max_level)
    var_dump(data, max_level or 20)
    recursion = {}  --释放内存
end

-- 解析一行代码
local function parse_line(line, last_class)
    -- 解析导出的库
    local lib_name, lib = string.match(line, lib_regex);
    if lib then
        libs[lib_name] = lib
        return
    end

    -- 解析导出类
    local class, class_name = string.match(line, class_regex)
    if class then
        last_class =
        {
            class = class,
            class_name = class_name,

            set = {},
            define = {}
        }
        assert(not classes[class_name])
        classes[class_name] = last_class
        return last_class
    end

    -- 解析导出函数
    local define, define_name = string.match(line, define_regex)
    if define then
        assert(not last_class.define[define_name])
        last_class.define[define_name] =
        {
            define = define,
            define_name = define_name
        }
        return last_class
    end

    -- 解析导出常量定义
    local set, set_name = string.match(line, set_regex)
    if set then
        assert(not last_class.set[set_name])
        last_class.set[set_name] =
        {
            set = set,
            set_name = set_name
        }
        return last_class
    end

    return last_class
end

-- 从lstate.cpp中解析要导出的api
local function parse_lib_class()
    local last_class
    for line in io.lines(dir .. entrance) do
        last_class = parse_line(line, last_class)
    end
end

-- 查找注释的类、函数、常量
local function parse_comment_name(line, last_comment)
    local class_name = string.match(line, "^class ([_%w]+)")
    if class_name then
        last_comment.class_name = class_name
        return true
    end

    local define_name = string.match(
        line, "^[static]*%s*int32_t%s+([_%w]+)%(lua_State %*L%)")
    if define_name then
        last_comment.define_name = define_name
        return true
    end

    -- 必须是空行，不然这个注释不是类、lua函数、常量的注释，没用的
    print(line)
    assert(string.match(line, "^%s*$"))

    return false
end

-- 从一行代码中解析注释
local function parse_comment_line(line, last_comment)
    -- 查找多行注释开始
    if not last_comment then
        local beg_cmt = string.match(line, "^%s*/%*%*")
        if beg_cmt then
            last_comment = { cmt = {}, param = {} }
            return last_comment
        end

        -- 查找单行注释
        local single_name, single_val, single_cmt =
            string.match(line, "^%s*([_%w]+)(.+)///<%s*(.+)")
        if single_name then
            local val = string.match(single_val, "%s*=%s([true%|%false%|%d]+)")
            last_comment =
            {
                val = val,
                set_name = single_name,
                cmt = { single_cmt }
            }
            return last_comment, true
        end

        return nil
    end

    -- 如果多行注释结束了，那么查找注释的类、函数、常量
    if last_comment.pending then
        local done = parse_comment_name(line, last_comment)
        return last_comment, done
    end

    -- 查找多行注释是否结束
    local end_cmt = string.match(line, "^%s*%*/")
    if end_cmt then
        last_comment.pending = true
        return last_comment
    end

    -- 查找多行注释内容
    local comment = string.match(line, "^%s*%*%s*(.+)")
    if comment then
        table.insert(last_comment.cmt, comment)

        local param = string.match(line, "^%s*%*%s*@param%s([_%w]+)")
        if param then table.insert(last_comment.param, param) end
    end

    return last_comment
end

-- 由于现在只是简单地通过正则解析符号
-- 对于 LUA_LIB_OPEN("util", luaopen_util); 这种库，是没办法知道它在哪个文件里
-- 声明，有哪些导出的函数的。现在的做法是，如果找到 luaopen_util 则认为该文件是
-- 这个库所在的文件，并且这个文件里所有通过dexygen注释导出的函数，都是这个库的接
-- 口
local function check_lib(line, file)
    if lib_file[file] then return end

    for lib_name, lib in pairs(libs) do
        if string.match(line,
            "^int32_t%s+" .. lib .. "%s*%(lua_State%s+%*L%)") then
            lib_file[file] = lib_name
        end
    end
end

-- 从单个文件中解析出注释
local function parse_file(file)
    --[[
        目前需要导出的注释是按doxygen标准的
        分两种:
        /**
         * 注释1
         */
        符号在注释下面

        符号在注释左边 ///< 注释2
    ]]
    local done
    local file_comment = {}
    local last_comment = nil
    for line in io.lines(file) do
        check_lib(line, file)
        last_comment, done = parse_comment_line(line, last_comment)
        if done then
            last_comment.pending = nil
            table.insert(file_comment, last_comment)
            last_comment = nil
        end
    end

    comments[file] = file_comment
end

-- 从源码中解析出注释
local function parse_comment()
    -- lua无法直接读取目录列表，只能借助shell
    -- win下使用dir指令
    -- for dir in io.popen([[dir "C:\Program Files\" /b /ad]]):lines() do
    --     print(dir)
    -- end
    local proc = io.popen(string.format("find %s -type f", dir))
    for file in proc:lines() do parse_file(file) end

    for _, file in pairs(other_file) do parse_file(file) end
end

-- 导出一个符号(常量定义、函数等)
local function export_one_symbol(file, base, indexer, symbol)
    for _, cmt in pairs(symbol.cmt) do
        file:write("-- ")
        file:write(cmt)
        file:write("\n")
    end

    if symbol.set_name then
        file:write(base)
        file:write(".")
        file:write(symbol.set_name)
        file:write(" = ")
        file:write(symbol.val)
    elseif symbol.define_name then
        file:write("function ")
        file:write(base)
        file:write(indexer)
        file:write(symbol.define_name)
        file:write("(")

        local first = true
        for _, param in pairs(symbol.param or {}) do
            if not first then file:write(", ") end
            first = false
            file:write(param)
        end

        file:write(")\nend")
    elseif symbol.class_name then
        file:write(base)
        file:write(" = {}")
    else
        assert(false)
    end
    file:write("\n\n")
end

-- 导出单个库
local function export_one_lib(lib_name)
    local file = io.open(export_dir .. string.lower(lib_name) .. ".lua", "wb")
    file:write(string.format("-- %s\n", lib_name))
    file:write("-- auto export by engine_api.lua do NOT modify!\n\n")

    file:write(string.format("%s = {}\n\n", lib_name))

    -- 查找当前库在哪个文件
    local file_path
    for path, name in pairs(lib_file) do
        if name == lib_name then
            file_path = path
            break;
        end
    end

    if not file_path then
        print(string.format("Warning: no file found for lib %s", lib_name))
        return
    end

    local file_cmt = comments[file_path]
    if not file_cmt then
        print(string.format("Warning: no symbol found for lib %s", lib_name))
        return
    end

    for _, symbol in pairs(file_cmt) do
        assert(not symbol.class_name)
        export_one_symbol(file, lib_name, ".", symbol)
    end
end

-- 导出库接口到文件
local function export_lib()
    for lib_name in pairs(libs) do
        export_one_lib(lib_name)
    end
end

-- 导出一个类型
-- @class_name: lua中的类名
-- @class:包含的类型信息，class.class是C++中的类型名
local function export_one_class(class_name, class)
    local file = io.open(
        export_dir .. string.lower(class_name) .. ".lua", "wb")
    file:write(string.format("-- %s\n", class_name))
    file:write("-- auto export by engine_api.lua do NOT modify!\n\n")

    local symbols
    local class_sym
    for _, file_cmts in pairs(comments) do
        for _, symbol in pairs(file_cmts or {}) do
            if class.class == symbol.class_name then
                class_sym = symbol
                symbols = file_cmts
                break
            end
        end
    end

    if not symbols then
        print(string.format(
            "Warning: no symbol found for class %s", class.class))
        return
    end

    export_one_symbol(file, class_name, nil, class_sym)
    for _, symbol in pairs(symbols) do
        if not symbol.class_name then
            export_one_symbol(file, class_name, ":", symbol)
        end
    end
end

-- 导出类
local function export_class()
    for class_name, class in pairs(classes) do
        export_one_class(class_name, class)
    end
end

local beg = os.clock()

parse_lib_class()
parse_comment()
export_lib()
export_class()

print("export done, time " .. os.clock() - beg)
