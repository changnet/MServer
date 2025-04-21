-- 基础日志接口
Log = {}

local g_async_log = g_async_log

local logger_print = g_async_log.print

local function to_readable(val)
    if type(val) == "string" then return "\"" .. val .. "\"" end

    return tostring(val)
end

--- @param var 要打印的数据
--- @param max_level table要展开打印的计数，默认nil表示全部展开
--- @param prefix 用于在递归时传递缩进，该参数不供用户使用于
local recursion = {}
local function var_dump(var, max_level, prefix)
    if type(prefix) ~= "string" then prefix = "" end

    if type(var) ~= "table" then
        print(prefix .. tostring(var))
    elseif recursion[var] then
        print(var, "dumplicate") -- 重复递归
    else
        recursion[var] = true

        local prefix_next = prefix .. "    "
        print(prefix .. "{")
        for k, v in pairs(var) do
            if type(v) ~= "table" or max_level <= 1 then
                print(prefix_next .. to_readable(k) .. " = " .. to_readable(v))
            else
                print(prefix_next .. to_readable(k) .. " = " .. tostring(v))
                var_dump(v, max_level - 1, prefix_next)
            end
        end
        print(prefix .. "}")
    end
end

-- var dump 打印任意变量的值，包括table
-- @param var 需要打印的变量
-- @param message 附加的消息，可为nil
function vd(var, message)
    if message then print(message) end
    var_dump(var, 96)
    recursion = {} -- 释放内存
end

-- 异步print log,只打印，不格式化。仅在日志线程开启后有效
function print(...)
    -- 128 = MASK_S_L，参考C++中的定义
    return logger_print(g_async_log, 128, ...)
end

-- 异步print format log,以第一个为format参数，格式化后面的参数.仅在日志线程开启后有效
function printf(fmt, ...)
    return logger_print(g_async_log, 128, string.format(fmt, ...))
end

-- 错误日志，写入根目录下的error文件 (参数不能带有nil参数)
function eprint(...)
    -- 129 = MASK_C_R|MASK_S_L，参考C++中的定义
    return g_async_log:error(129, table.concat({...}, "    "))
end

--  格式化日志并写入根目录下的lua_error.txt文件
function eprintf(fmt, ...)
    return g_async_log:error(129, string.format(fmt, ...))
end

-- 异步print log,只打印，不格式化。仅在日志线程开启后有效
function warn(...)
    -- 136 = MASK_C_Y | MASK_S_L，参考C++中的定义
    return logger_print(g_async_log, 136, ...)
end

-- 异步print format log,以第一个为format参数，格式化后面的参数.仅在日志线程开启后有效
function warnf(fmt, ...)
    return logger_print(g_async_log, 136, string.format(fmt, ...))
end

-- 以红色打印日志
function Log.red(...)
    return logger_print(g_async_log, 129, ...)
end

-- 以红色格式化江打印日志
function Log.redf(fmt, ...)
    return logger_print(g_async_log, 129, string.format(fmt, ...))
end

-- 以红色打印日志
function Log.green(...)
    return logger_print(g_async_log, 130, ...)
end

-- 以红色格式化江打印日志
function Log.greenf(fmt, ...)
    return logger_print(g_async_log, 130, string.format(fmt, ...))
end

-- 以蓝色打印日志
function Log.blue(...)
    return logger_print(g_async_log, 132, ...)
end

-- 以蓝色格式化江打印日志
function Log.bluef(fmt, ...)
    return logger_print(g_async_log, 132, string.format(fmt, ...))
end

-- 以黄色打印日志
function Log.yellow(...)
    return logger_print(g_async_log, 136, ...)
end

-- 以黄色格式化江打印日志
function Log.yellowf(fmt, ...)
    return logger_print(g_async_log, 136, string.format(fmt, ...))
end
