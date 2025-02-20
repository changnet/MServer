-- main.lua
-- 2018-04-04
-- xzc
-- 进程入口文件(此文件不可热更)

local util = require "engine.util"

-- 简单模拟c的getopt函数，把参数读出来，按table返回
local function get_opt(args)
    local opts = {}
    for _, opt in pairs(args) do
        -- 目前仅支持'--'开头的长选项，不支持'-'开头的短选项
        local b1, b2 = string.byte(opt, 1, 2)
        if 45 ~= b1 and 45 ~= b2 then error("invalid argument: " .. opt) end

        -- 按"="号拆分参数(--node=gateway)，也可能没有等号(--daemon)
        local k, v = string.match(opt, "^%-%-(%w+)=?([%w ;._]*)")
        if not k or not v or v == "" then
            error("invalid argument: " .. opt)
        end

        opts[k] = v -- 如果没有等号，v为一个空字符串
    end

    return opts
end

local function main(cmd, ...)
    local args = {...}
    local cmd_args = table.concat(args, " ") -- command line

    local opts = get_opt(args)
    local node = assert(opts.node, "missing argument --node")

    util.mkdir_p("log") -- 创建日志目录
    util.mkdir_p("runtime") -- 创建运行时数据存储目录
    util.mkdir_p("runtime/rank") -- 创建运行时排行榜数据存储目录

    -- win下设置cmd为utf8编码
    -- 并设置标题用于区分cmd界面
    if WINDOWS and not opts.deamon then
        os.execute("chcp 65001")
        os.execute("title " .. cmd_args)
    end

    local cwd = util.getcwd()
    local srv_dir =  string.match(cwd, "(.*)[/\\]") -- 不带最后一个/
    g_env:set("node", node)
    g_env:set("cwd", cwd) -- current working directory
    g_env:set("cmd_args", cmd_args) -- command line args
    g_env:set("srv_dir", srv_dir) -- server source code directory
    for k, v in pairs(opts) do
        g_env:set(k, v)
    end

    -- gateway1去数字
    local file_name = string.match(node, "(%a+)")

    -- 未设置package.path，只能用dofile不能用require
    dofile(srv_dir .. "/src/engine/bootstrap.lua") -- 加载启动器
    dofile(srv_dir .. "/src/process/p_" .. file_name .. ".lua") -- 加载进程逻辑入口
end

main(...)
