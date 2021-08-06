-- global.lua
-- 2015-09-07
-- xzc
-- 常用的全局函数

local async_logger = g_async_log

-- 用于存储全局临时数据
__global_storage = __global_storage or {}

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

-- 用于lua错误信息处理
function __G__TRACKBACK(msg, co)
    local stack_trace = debug.traceback(co)

    local str = tostring(msg) .. "\n" .. stack_trace
    async_logger:elog(str)
    return msg
end

-- 用于底层C错误信息处理
function __G_C_TRACKBACK(msg, co)
    local stack_trace = debug.traceback(co)

    return tostring(msg) .. "\n" .. stack_trace
end

-- 异步print log,只打印，不格式化。仅在日志线程开启后有效
function print(...)
    return async_logger:plog(...)
end

-- 异步print format log,以第一个为format参数，格式化后面的参数.仅在日志线程开启后有效
function printf(fmt, ...)
    return async_logger:plog(string.format(fmt, ...))
end

-- 错误处调用 直接写入根目录下的lua_error.txt文件 (参数不能带有nil参数)
function elog(...)
    return async_logger:elog(table.concat_any("    ", ...))
end

-- 断言，支持多个message参数
function assert(expr, ...)
    if expr then return expr end

    local msg = table.concat_any("    ", ...)
    return error(msg)
end

-- 创建一块数据存储空间，仅仅是为了方便第一管理，热更，本地化
function global_storage(key, def_val)
    local storage = __global_storage[key]
    if not storage then
        storage = def_val or {}
        __global_storage[key] = storage
    end

    return storage
end
