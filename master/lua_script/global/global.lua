--global.lua
--2015-09-07
--xzc

--常用的全局函数

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
            if type(v) ~= "table" or (type(max_level) == "number" and max_level <= 1) then
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

-- 时间字符串。用的不是ev:now()，故只能用于错误打印，不能用于游戏逻辑
local function time_str()
    return os.date("%Y-%m-%d %H:%M:%S", os.time())
end

local function lualog (...)
    print(string.format(...))
end

local function writeTrackFileLog(debug_traceback)
    local file = io.open("lua_crash.txt","a+")
    if not file then -- 无写入权限...
        print(debug_traceback)
        return
    end

    file:write(debug_traceback)
    file:write("\r\n")
    file:close()
end

local function writeErrorLog(msg)
    local file = io.open("lua_error.txt","a+")
    file:write(msg)
    file:write("\r\n")
    file:close()
end

-- for CCLuaEngine traceback
function __G__TRACKBACK__(msg)
    local debug_traceback = debug.traceback()
    lualog("LUA ERROR: " .. tostring(msg) .. "\n")
    lualog(debug_traceback)
    writeTrackFileLog(debug_traceback)
end

--只打印不写入文件(参数不能带有nil参数,不能带有table,带有table的可以先用json.encode转成string)
function PLOG(...)
    local temp = {...}

    if table.size(temp) == 0 then
        print("PLOG can set nil")
        return
    end

    local is_t = false
    local str = ""
    for _,v in ipairs(temp) do
        if type(v) == "table" then
            is_t = true
            local st = Json.encode(v)
            str = str .. "_" .. st
        else
            str = str .. "_" .. v
        end
    end

    if is_t == true then
        str = "<<< Lua >>>--------------------" .. "PLOG can not have lua table or nil [" .. str .. "]"
        print(str)
        writeErrorLog(str)
        return
    end

    print("<<< Lua >>>--------------------" .. string.format(...))
end

--错误处调用 直接写入根目录下的lua_error.txt文件 (参数不能带有nil参数)
function ELOG(...)
    local temp = {...}

    if table.size(temp) == 0 then
        print("PLOG can set nil")
        return
    end
    
    local is_t = false
    local str = ""
    for _,v in ipairs(temp) do
        if type(v) == "table" then
            is_t = true
            local st = Json.encode(v)
            str = str .. "_" .. st
        else
            str = str .. "_" .. v
        end
    end

    if is_t == true then
        str = "<<< Lua >>>--------------------" .. "PLOG can not have lua table or nil object >>" .. str
        print(str)
        writeErrorLog(str)
        return
    end

    local ss = "<<< LUA ERROR >>>--------------------" .. time_str() .. " " .. string.format(...)
    print(ss)
    writeErrorLog(ss)
end

-- 热更函数接口
function require_ex( path )
    --[[
    单例对象返回一个实例，此实例此时不一定有其他引用。故需要_instance来引用以防被销毁
    重新require后实例会成为upvalue，不会被销毁
    ]]
    local _instance = package.loaded[path]
    package.loaded[path] = nil
    return require(path)
end
