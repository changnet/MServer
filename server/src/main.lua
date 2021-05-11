-- main.lua
-- 2018-04-04
-- xzc

-- 进程入口文件(引文件不可热更)

-- 设置lua文件搜索路径
package.path = "../?.lua;" .. "../src/?.lua;" .. package.path
-- 设置c库搜索路径，用于直接加载so或者dll的lua模块
package.cpath = "../c_module/?.so;" .. package.cpath

local Log  = require "Log"
local util = require "util"

require "global.oo" -- 这个文件不能热更
require "global.require" -- 需要热更的文件，必须放在这后面

require "global.global" -- 这个要放require后面，它是可以热更的

-- 当前进程application对象
g_app = nil

-- 简单模拟c的getopt函数，把参数读出来，按table返回
local function get_opt(...)
    local opts = {}
    local raw = { ... }
    for _, opt in pairs(raw) do
        -- 目前仅支持'--'开头的长选项，不支持'-'开头的短选项
        local b1, b2 = string.byte(opt, 1, 2)
        if 45 ~= b1 and 45 ~= b2 then error("invalid argument: " .. opt) end

        -- 按"="号拆分参数(--app=gateway)，也可能没有等号(--daemon)
        local k, v = string.match(opt, "^%-%-(%w+)=?([%w ]*)")
        if not k then error("invalid argument: " .. opt) end

        opts[k] = v -- 如果没有等号，v为一个空字符串
    end

    return opts, raw
end

-- 打印内核参数
local function log_app_info(opts)
    PRINT("#####################################################")
    PRINTF("## starting %s", table.concat(opts, " "))
    PRINTF("## OS: %s", __OS_NAME__)
    PRINTF("## %s", _VERSION)
    PRINTF("## backend: %s", __BACKEND__)
    PRINTF("## complier: %s %s", __COMPLIER_, __VERSION__)
    PRINTF("## build time: %s", __TIMESTAMP__)
    PRINT("#####################################################")
end

local function main( cmd, ... )
    local opts, raw_opts = get_opt(...)
    math.randomseed( ev:time() )

    util.mkdir_p( "log" )  -- 创建日志目录
    util.mkdir_p( "runtime" ) -- 创建运行时数据存储目录
    util.mkdir_p( "runtime/rank" ) -- 创建运行时排行榜数据存储目录

    local name = assert(opts.app, "missing argument --app")

    ev:set_app_ev(250)
    -- 设置主循环临界时间，目前只用来输出日志,检测卡主循环
    ev:set_critical_time(1000)

    -- 设置错误日志、打印日志
    -- 如果你的服务器是分布式的，包含多个进程，则注意名字要区分开来
    -- win下文件名不支持特殊字符的，比如":"
    local epath = string.format("log/%s%02d_error", name, opts.index or 0)
    local ppath = string.format("log/%s%02d_runtime", name, opts.index or 0)
    Log.set_std_option(opts.deamon, ppath, epath) -- 设置C++用的日志参数
    g_async_log:set_option(ppath, Log.PT_DAILY)
    g_async_log:set_option(epath, Log.PT_SIZE, 1024 * 1024 * 10)

    -- 非后台模式，打印进程名到屏幕，否则多个进程在同一终端开户时不好区分日志
    -- 取首字母大写即可，不用全名，如果以后有重名再改
    -- local app_name = string.format("%s.%d",name,index)
    local app_name = string.format("%s%d",
        string.char(string.byte(name) - 32), opts.index or 0)
    if not opts.deamon then Log.set_name( app_name ) end

    log_app_info(raw_opts)

    local App = require( string.format( "%s.app",name ) )

    g_app = App( cmd, opts )
    g_app:exec()
end

-- 由于日志线程未启动，不能直接使用__G__TRACKBACK
local function __traceback( msg,co )
    local stack_trace = debug.traceback( co )
    local info_table = { tostring(msg),"\n",stack_trace }

    print(table.concat( info_table ))
end

xpcall(main, __traceback, ...)
