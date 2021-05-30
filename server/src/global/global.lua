-- global.lua
-- 2015-09-07
-- xzc
-- 常用的全局函数
local async_logger = g_async_log

local function to_readable(val)
    if type(val) == "string" then return "\"" .. val .. "\"" end

    return val
end

--- @param data 要打印的字符串
--- @param [max_level] table要展开打印的计数，默认nil表示全部展开
--- @param [prefix] 用于在递归时传递缩进，该参数不供用户使用于
local recursion = {}
local function var_dump(data, max_level, prefix)
    if type(prefix) ~= "string" then prefix = "" end

    if type(data) ~= "table" then
        print(prefix .. tostring(data))
    elseif recursion[data] then
        print(data, "dumplicate") -- 重复递归
    else
        recursion[data] = true
        print(data)

        local prefix_next = prefix .. "    "
        print(prefix .. "{")
        for k, v in pairs(data) do
            io.stdout:write(prefix_next .. tostring(to_readable(k)) .. " = ")
            if type(v) ~= "table" or
                (type(max_level) == "number" and max_level <= 1) then
                print(to_readable(v))
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
    recursion = {} -- 释放内存
end

-- 用于lua错误信息处理
function __G__TRACKBACK(msg, co)
    local stack_trace = debug.traceback(co)
    local info_table = {tostring(msg), "\n", stack_trace}
    local str = table.concat(info_table)

    Log.elog(str)
    return msg
end

-- 用于底层C错误信息处理
function __G_C_TRACKBACK(msg, co)
    local stack_trace = debug.traceback(co)
    local info_table = {tostring(msg), "\n", stack_trace}

    return table.concat(info_table)
end

-- 异步print log,只打印，不格式化。仅在日志线程开启后有效
function PRINT(any, ...)
    local args_num = select("#", ...)
    if 0 == args_num then return async_logger:plog(tostring(any)) end

    -- 如果有多个参数，则合并起来输出，类似Lua的print函数
    async_logger:plog(table.concat_any("    ", any, ...))
end

-- 异步print format log,以第一个为format参数，格式化后面的参数.仅在日志线程开启后有效
function PRINTF(fmt, any, ...)
    -- 默认为c方式的print字符串格式化打印方式
    if any and "string" == type(fmt) then
        return async_logger:plog(string.format(fmt, any, ...))
    else
        return PRINT(fmt, any, ...)
    end
end

-- 错误处调用 直接写入根目录下的lua_error.txt文件 (参数不能带有nil参数)
function ERROR(fmt, any, ...)
    if any and "string" == type(fmt) then
        return async_logger:elog(string.format(fmt, any, ...))
    end

    if any then
        async_logger:elog(table.concat_any("    ", fmt, any, ...))
    else
        async_logger:elog(tostring(fmt))
    end
end

function ASSERT(expr, ...)
    if expr then return expr end

    local msg = table.concat_any("    ", ...)
    return error(msg)
end

-- 从一个文件加载全局定义，该文件必须是未require的,里面的全局变量必须是未定义的
-- @param path 需要加载的文件路径，同require的参数，一般用点号
-- @param g 是否设置到全局
-- @return table,包含该文件中的所有全局变量定义
function load_global_define(path, g)
    local _g_defines = _G._g_defines
    if not _g_defines then
        _g_defines = {}
        _G._g_defines = _g_defines
    end
    if _g_defines[path] then
        -- 必须先清除旧的变量，否则__newindex不会触发
        for k in pairs(_g_defines[path]) do _G[k] = nil end
    end

    local defines = {}
    setmetatable(_G, {
        __newindex = function(t, k, v)
            rawset(defines, k, v)
            if g then rawset(t, k, v) end
        end
    })
    require(path)
    setmetatable(_G, nil)

    _g_defines[path] = defines
    return _g_defines[path]
end
