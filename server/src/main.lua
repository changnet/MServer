-- main.lua
-- 2018-04-04
-- xzc

-- 进程入口文件(引文件不可热更)

-- 设置lua文件搜索路径
package.path = "../?.lua;" .. "../src/?.lua;" .. package.path
-- 设置c库搜索路径
package.cpath = "../c_module/?.so;" .. package.cpath

require "global.oo"
require "global.require" -- 需要热更的文件，必须放在这后面

local Log  = require "Log"
local util = require "util"
require "global.global" -- 这个要放require后面，它是可以热更的
local time = require "global.time"

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
        local k, v = string.match(opt, "^%-%-(%w+)=?(%w*)")
        if not k then error("invalid argument: " .. opt) end

        opts[k] = v -- 如果没有等号，v为一个空字符串
    end

    return opts, raw
end

local function main( cmd, ... )
    local opts, raw_opts = get_opt(...)
    math.randomseed( ev:time() )

    util.mkdir_p( "log" )  -- 创建日志目录
    util.mkdir_p( "runtime" ) -- 创建运行时数据存储目录
    util.mkdir_p( "runtime/rank" ) -- 创建运行时排行榜数据存储目录

    local name = assert(opts.app, "miss argument --app")

    -- 设置主循环临界时间，目前只用来输出日志,检测卡主循环
    ev:set_critical_time(1000)

    -- 设置错误日志、打印日志
    -- 如果你的服务器是分布式的，包含多个进程，则注意名字要区分开来
    local tm = time.ctime()

    -- win下文件名不支持特殊字符的，比如":"
    local epath = string.format( "log/%s_error",name )
    local mpath = string.format( "log/%s_mongo",name )
    local ppath = string.format(
        "log/%s#%04d-%02d-%02d#%02d_%02d_%02d",
        name,tm.year,tm.month,tm.day,tm.hour,tm.min,tm.sec )
    Log.set_args( opts.deamon, ppath, epath, mpath )

    -- 非后台模式，打印进程名到屏幕，否则多个进程在同一终端开户时不好区分日志
    -- 取首字母大写即可，不用全名，如果以后有重名再改
    -- local app_name = string.format("%s.%d",name,index)
    local app_name = string.format("%s%d",
        string.char(string.byte(name) - 32), opts.index or 0)
    if not opts.deamon then Log.set_name( app_name ) end

    SYNC_PRINTF("starting %s", table.concat(raw_opts, " "))
    local App = require( string.format( "%s.app",name ) )

    g_app = App( cmd, opts )
    g_app:exec()
end

xpcall( main, __G__TRACKBACK,... )
