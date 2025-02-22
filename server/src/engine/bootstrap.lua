-- 启动(此文件不热更)
Bootstrap = {}

local buddha = [[

                  _oo0oo_
                 o8888888o
                 88" . "88
                 (| -_- |)
                 0\  =  /0
               ___/`---'\___
             .' \\|     |// '.
            / \\|||  :  |||// \
           / _||||| -:- |||||- \
          |   | \\\  -  /// |   |
          | \_|  ''\---/''  |_/ |
          \  .-\__  '-'  ___/-. /
        ___'. .'  /--.--\  `. .'___
     ."" '<  `.___\_<|>_/___.' >' "".
    | | :  `- \`.;`\ _ /`;.`/ - ` : | |
    \  \ `_.   \_ __\ /__ _/   .-` /  /
=====`-.____`.___ \_____/___.-`___.-'=====
                  `=---='

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

          佛祖保佑         永无BUG
]]
--//////////////////////////////////////////////////////////////////////////////

-- 这些函数会被覆盖，但原函数保留一下，某些特殊逻辑需要
__print = print
__error = error
__assert = assert
__require = require
__package_path = package.path -- 保留原path
__package_cpath = package.cpath

-- 底层通用工具
Util = require "engine.util"

-- 宽度8刚好可以支持gateway1，如果节点数量再多就要扩展，否则日志就不对齐了
local LOG_WIDTH = 8

-- 打印运行的环境参数
local function log_env_info()
    print("#####################################################")
    printf("## starting %s", g_env:get("cmd_args"))
    printf("## OS: %s", g_env:get("__OS_NAME__"))
    printf("## %s", _VERSION)
    printf("## backend: %s", g_env:get("__BACKEND__"))
    printf("## complier: %s", g_env:get("__COMPLIER_"))
    printf("## build time: %s", g_env:get("__TIMESTAMP__"))
    printf("## NET: %s", g_env:get("IPV4") or g_env:get("IPV6"))
    printf("## cwd: %s", g_env:get("cwd"))
    print("#####################################################")
    print(buddha)
end

local function set_path()
    -- lua加载文件可以自动识别/和\，所以这里统一用linux的就行
    -- 这里只保留源码目录，不保留系统目录
    -- 所有的lua文件、so插件必须仅在源码目录搜索，避免版本冲突问题

    local srv_dir = g_env:get("srv_dir")

    -- 设置lua文件搜索路径
    package.path = srv_dir .. "/src/?.lua;" .. srv_dir .. "/src/modules/?.lua;"
    -- 设置c库搜索路径，用于直接加载so或者dll的lua模块
    if WINDOWS then
        package.cpath = "../c_module/?.dll;"
    else
        package.cpath = "../c_module/?.so;"
    end
end

-- 设置进程的日志参数
local function set_process_log()
    -- 设置错误日志、打印日志
    -- 名字需要能区分不同进程名，不同节点
    -- win下文件名不支持特殊字符的，比如":"

    local node = g_env:get("node")
    local epath
    local ppath
    local node_name, node_id = string.match(node, "(%a+)(%d*)")
    if node_id ~= "" then
        -- 以集群节点启动，比如gateway10
        epath = string.format("log/%s%03d_error", node_name, tonumber(node_id))
        ppath = string.format("log/%s%02d_runtime", node_name, tonumber(node_id))
    else
        epath = string.format("log/%s_error", node_name)
        ppath = string.format("log/%s_runtime", node_name)
    end

    local Log = require "engine.Log"
    Log:set_std_option(g_env:get("deamon"), ppath, epath) -- 设置C++用的日志参数
    g_async_log:set_option(ppath, Log.PT_DAILY)
    g_async_log:set_option(epath, Log.PT_SIZE, 1024 * 1024 * 10)

    -- 日志中需要打印进程名，否则多个进程在同一终端开户时不好区分日志
    Log:set_name(string.format(string.format("%%%ds", LOG_WIDTH), node))
end

-- 进程预加载必要的组件
function Bootstrap.process_preload()
    set_process_log()

    g_thread = g_mthread
    set_path()

    require "global.oo" -- 这个文件不能热更
    require "global.require" -- 重写require，后续用require加载的文件，都被标记为可热更
    require "global.global" -- 加载log函数
    require "system.define" -- 基础定义

    log_env_info()

    require "engine.engine"
    require "worker.worker"

    LOCAL_ADDR = PROCESS_ADDR
    Engine.add_thread_ctx(LOCAL_ADDR, g_mthread:toludata())

    require "engine.co_pool"
    require "message.thread_message"
    require "engine.signal"
    require "engine.shutdown"
    require "global.rtti"
    require "rpc.rpc"
    require "timer.timer"
    require "network.socket_mgr"

    Signal.mask(2, Shutdown.begin)
    Signal.mask(15, Shutdown.begin)

    math.randomseed(os.time())
end

-- worker预加载必要的组件
function Bootstrap.worker_preload(addr)
    set_path()

    require "global.oo" -- 这个文件不能热更
    require "global.require" -- 重写require，后续用require加载的文件，都被标记为可热更
    require "global.global" -- 加载log函数
    require "system.define" -- 基础定义

    require "engine.engine"
    require "worker.worker"

    local wtype, index = Engine.unmake_address(addr)

    local Log = require "engine.Log"
    Log:set_name(string.format(string.format("%%%ds", LOG_WIDTH),
        string.format("%s%d", Worker.type_name(wtype), index)))

    LOCAL_ADDR = addr
    LOCAL_TYPE = wtype
    Engine.add_thread_ctx(addr, g_thread:toludata())

    require "engine.co_pool"
    require "message.thread_message"
    require "engine.signal"
    require "engine.shutdown"
    require "global.rtti"
    require "rpc.rpc"
    require "timer.timer"
    require "network.socket_mgr"

    math.randomseed(os.time())
end

function Bootstrap.process_start(worker_setting)
    for _, s in ipairs(worker_setting) do
        Worker.create(s)
    end
end
