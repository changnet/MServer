--global.lua
--2015-09-07
--xzc

--常用的全局函数

local Log = require "Log"
local util = require "util"

local async_logger = Log()

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

function __G__TRACKBACK__( msg,co )
    local stack_trace = debug.traceback( co )
    local info_table = { tostring(msg),"\n",stack_trace }
    local str = table.concat( info_table )

    Log.elog( str )
end

-- 同步print log,只打印，不格式化
function SYNC_PRINT( any,... )
    local args_num = select( "#",... )

    -- if not ( ... ) then ... end 这种写法当...的第一个参数是nil就不对了
    if 0 == args_num then return Log.plog( tostring(any) ) end

    -- 如果有多个参数，则合并起来输出，类似Lua的print函数
    Log.plog( table.concat_any( "    ",any,... ) )
end

-- 同步print format log,以第一个为format参数，格式化后面的参数
function SYNC_PRINTF( fmt,any,... )
    -- 默认为c方式的print字符串格式化打印方式
    if any and "string" == type( fmt ) then
        Log.plog( string.format( fmt,any,... ) )
    else
        SYNC_PRINT( fmt,any,... )
    end
end

-- 异步print log,只打印，不格式化。仅在日志线程开启后有效
function PRINT( any,... )
    local args_num = select( "#",... )
    if 0 == args_num then
        return async_logger:write( "",tostring(any),2 )
    end

    -- 如果有多个参数，则合并起来输出，类似Lua的print函数
    async_logger:write( "",table.concat_any( "    ",any,... ),2 )
end

-- 异步print format log,以第一个为format参数，格式化后面的参数.仅在日志线程开启后有效
function PRINTF( fmt,any,... )
    -- 默认为c方式的print字符串格式化打印方式
    if any and "string" == type( fmt ) then
        return async_logger:write( "",string.format( fmt,any,... ),2 )
    else
        return PRINT( fmt,any,... )
    end
end

--错误处调用 直接写入根目录下的lua_error.txt文件 (参数不能带有nil参数)
function ELOG( fmt,any,... )
    local info_table = nil
    if any and "string" == type( fmt ) then
        return Log.elog( string.format( fmt,any,... ) )
    end

    if any then
        Log.elog( table.concat_any( "    ",fmt,any,... ) )
    else
        Log.elog( tostring(fmt) )
    end
end

function ASSERT( expr,... )
    if expr then return end

    local msg = table.concat_any( "    ",... )
    return error(msg)
end

--测试时间,耗时打印
local _sec, _usec -- 这函数热更会导致出错，仅测试用
function f_tm_start()
    _sec, _usec = util.timeofday()
end

--[[
1秒＝1000毫秒，
1毫秒＝1000微秒，
1微妙＝1000纳秒，
1纳秒＝1000皮秒。
秒用s表现,毫秒用ms,微秒用μs表示，纳秒用ns表示，皮秒用ps表示
]]
function f_tm_stop(...)
    local sec,usec = util.timeofday()
    assert( sec >= _sec,"time jump" )
    local temp =  math.floor( (sec-_sec)*1000000 + usec - _usec )
    print(...,temp,"microsecond")
end
