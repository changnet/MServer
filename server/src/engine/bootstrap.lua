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
__loader = nil -- 当前进程或者worker功能加载入口文件
__package_path = package.path -- 保留原path
__package_cpath = package.cpath

-- 底层通用工具
Util = require "engine.util"

-- 服务器设置文件
g_setting = nil

-- 当前宽度刚好可以支持gateway1，如果名字再长就要扩展，否则日志就不对齐了
local LOG_WIDTH = 9

-- 需要按优先级启动的模块
local boot_modules = nil

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

-- 加载游戏设置文件
local function load_setting()
    local path = g_env:get("--file")
    if not path then
        -- 未从命令行传设置文件路径，则读取默认路径
        local srv_dir = g_env:get("srv_dir")
        path = srv_dir .. "/setting/setting.lua"
    end

    g_setting = dofile(path)
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

    local node = g_env:get("--node")
    local epath
    local ppath
    local local_name
    local node_name, node_id = string.match(node, "(%a+)(%d*)")
    if node_id ~= "" then
        -- 以集群节点启动，比如gateway10，这时候主线程的日志名和worker就会重复，转为大写
        local_name = string.upper(node_name)
        epath = string.format("log/%s%03d_error", node_name, tonumber(node_id))
        ppath = string.format("log/%s%03d_info", node_name, tonumber(node_id))
    else
        local_name = node_name
        epath = string.format("log/%s_error", node_name)
        ppath = string.format("log/%s_info", node_name)
    end

    if g_env:get("deamon") then
        -- 后台模式运行，不需要输出日志到stdout，效率高一点点
        g_async_log:add_device("info", ppath, 1, 1)
        g_async_log:add_device("error", epath, 1, 2, 1024 * 1024 * 10)
    else
        g_async_log:add_device("stdout", "stdout", 1, 1)
        g_async_log:add_device("info", ppath, 1, 1, 0, "stdout")
        g_async_log:add_device("error", epath, 1, 2, 1024 * 1024 * 10, "stdout")
    end

    -- 日志中需要打印线程名，否则多个进程在同一终端开户时不好区分日志
    g_async_log:set_name(string.format(
        string.format("%%%ds", LOG_WIDTH), local_name))
    return local_name
end

-- 进程预加载必要的组件
function Bootstrap.process_init(loader)
    local local_name = set_process_log()

    g_thread = g_mthread
    set_path()

    require "global.oo" -- 这个文件不能热更
    require "global.require" -- 重写require，后续用require加载的文件，都被标记为可热更
    require "global.global" -- 加载错误处理函数
    require "global.log" -- 加载log函数
    require "system.define" -- 基础定义

    log_env_info()
    load_setting()

    require "engine.engine"
    require "worker.worker"

    LOCAL_ADDR = PROCESS_ADDR
    LOCAL_NAME = local_name
    Engine.add_thread_ctx(LOCAL_ADDR, g_mthread:toludata())

    require "engine.preloader"

    Signal.mask(2, Shutdown.process_stop)
    Signal.mask(15, Shutdown.process_stop)

    math.randomseed(os.time())

    __loader = loader
    Timer.timeout(0, function()
        if loader then require(loader) end
        Bootstrap.start()
    end)
end

-- worker预加载必要的组件
function Bootstrap.worker_init(addr, loader)
    set_path()

    require "global.oo" -- 这个文件不能热更
    require "global.require" -- 重写require，后续用require加载的文件，都被标记为可热更
    require "global.global" -- 加载错误处理函数
    require "global.log" -- 加载log函数
    require "system.define" -- 基础定义

    require "engine.engine"
    require "worker.worker"

    local wtype, index = Engine.unmake_address(addr)
    LOCAL_ADDR = addr
    LOCAL_TYPE = wtype
    LOCAL_NAME = string.format("%s%d", Worker.type_name(wtype), index)

    g_async_log:set_name(string.format(
        string.format("%%%ds", LOG_WIDTH), LOCAL_NAME))

    Engine.add_thread_ctx(addr, g_thread:toludata())

    load_setting()

    require "engine.preloader"

    math.randomseed(os.time())

    __loader = loader
    Timer.timeout(0, function()
        require(loader)
        Bootstrap.start()
    end)
end

-- 注册按优先级启动的模块
-- @param mod 需要启动的模块，包括boot_start、boot_ready函数
-- @param priority 启动优先级，越小优先级越高，默认20
function Bootstrap.reg(mod, priority)
    if not boot_modules then
        local PriorityManager = require "util.priority_manager"
        boot_modules = PriorityManager()
    end

    boot_modules:push(mod, priority)
end

-- worker启动完成
function Bootstrap.on_worker_ready(addr)
    assert(LOCAL_ADDR == PROCESS_ADDR)

    -- 同步worker到其他worker，加快worker间的消息交互
    -- 否则都会先抛给主线程，再由主线程转发
    for other_addr, w in pairs(WorkerHash) do
        if other_addr ~= addr and not w.cluster_worker then
            Send.Worker.on_ready(other_addr, addr)
        end
    end
end

local function boot_ready()
    if boot_modules and boot_modules.timer then
        Timer.stop(boot_modules.timer)
    end
    boot_modules = nil

    if LOCAL_ADDR ~= PROCESS_ADDR then
        -- 同步到线程，当前worker启动完成
        Send.Bootstrap.on_worker_ready(PROCESS_ADDR, LOCAL_ADDR)
        printf("worker %s ready, addr = %d",
            Worker.addr_name(LOCAL_ADDR), LOCAL_ADDR)
    else
        print("procsss ready")
    end
    if SE then SE.fire_event(SE_READY) end
end

local function boot_next_modules()
    local list = boot_modules:next()
    if not list then
        boot_ready()
        return true
    end

    if not boot_modules.wait then boot_modules.wait = {} end

    local all_ready = true
    for _, mod in ipairs(list) do
        local name = mod.name or "unknow"
        if not mod.boot() then
            all_ready = false
            boot_modules.wait[mod] = true
            printf("booting %s ...", name)
        else
            printf("booting %s ready", name)
        end
    end

    if all_ready then
        return boot_next_modules()
    end
    return all_ready
end

local function checkModuleReady()
    local wait = boot_modules.wait

    for mod in pairs(wait) do
        local name = mod.name or "unknow"
        if not mod.ready() then
            printf("booting %s ...", name)
            return
        else
            wait[mod] = nil
            printf("booting %s ready", name)
        end
    end

    if not next(wait) then boot_next_modules() end
end

-- 按优先级启动各个模块，启动完成后触发SE_READY事件
function Bootstrap.start()
    if not boot_modules then
        return boot_ready()
    end

    if boot_next_modules() then return end

    boot_modules.timer = Timer.interval(
        1000, 1000, -1, Rtti.temp_func(checkModuleReady))
end
