-- 启动(此文件不热更)
Startup = {}

--//////////////////////////////////////////////////////////////////////////////

-- 这些函数会被覆盖，但原函数保留一下，某些特殊逻辑需要
__print = print
__error = error
__assert = assert
__loader = nil -- 当前进程或者worker功能加载入口文件
__package_path = package.path -- 保留原path
__package_cpath = package.cpath

-- 底层通用工具
Util = require "engine.util"

-- 服务器设置文件
g_setting = nil
g_ready = false -- 当前线程是否启动完

-- 需要按优先级启动的模块
local startup_modules = nil

-- 打印运行的环境参数
local function log_env_info()
    print("#####################################################")
    printf("## starting %s", g_env:get("cmd_args"))
    printf("## OS: %s", g_env:get("__OS_NAME__"))
    printf("## %s", _VERSION)
    printf("## backend: %s", g_env:get("__BACKEND__"))
    printf("## complier: %s", g_env:get("__COMPLIER_"))
    printf("## build time: %s", g_env:get("__TIMESTAMP__"))
    printf("## cwd: %s", g_env:get("cwd"))
    printf("## exe: %s", g_env:get("exe"))
    print("#####################################################")

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
    print(buddha)
end

-- 加载游戏设置文件
local function load_setting()
    local path = g_env:get("--setting")
    if not path then
        -- 未从命令行传设置文件路径，则读取默认路径
        local cwd = g_env:get("cwd")
        path = cwd .. "/setting.lua"
    end

    g_setting = dofile(path)
end

local function set_path()
    -- lua加载文件可以自动识别/和\，所以这里统一用linux的就行
    -- 这里只保留源码目录，不保留系统目录
    -- 所有的lua文件、so插件必须仅在源码目录搜索，避免版本冲突问题

    local source = g_env:get("source")

    -- 设置lua文件搜索路径(常用的路径排前面，会影响require性能)
    package.path = source .. "src/modules/?.lua;"
        .. source .. "src/?.lua;"
        .. source .. "?.lua;" -- config.xxx

    -- 设置c库搜索路径，用于直接加载so或者dll的lua模块
    if WINDOWS then
        package.cpath = source .. "c_module/?.dll;"
    else
        package.cpath = source .. "c_module/?.so;"
    end
end

local function format_log_name(name, index, is_proc)
    local fullname
    if is_proc then
        fullname = string.format("%s_%d", name, index)
    else
        fullname = string.format("%s%d", name, index)
    end
    -- 当前宽度刚好可以支持gateway1，如果名字再长就要扩展，否则日志就不对齐了
    g_async_log:set_name(string.format(" %-8s", fullname))

    return fullname
end

-- 设置进程的日志参数
local function set_process_log(node_name, node_index)
    -- 设置错误日志、打印日志
    -- 名字需要能区分不同进程名，不同节点
    -- win下文件名不支持特殊字符的，比如":"

    -- 所有日志都输出到同一个文件，方便查看
    local path = string.format("log/%s%03d", node_name, node_index)
    -- 错误日志除了输出到通用日志文件，还单独输出error文件，用于触发运维邮件、电话
    local epath = string.format("log/%s%03d_error", node_name, node_index)

    if g_setting.deamon then
        -- 后台模式运行，不需要输出日志到stdout，效率高一点点
        g_async_log:add_device("info", path, 1, 1)
    else
        g_async_log:add_device("stdout", "stdout", 1, 1)
        g_async_log:add_device("info", path, 1, 1, 0, "stdout")
    end
    g_async_log:add_device("error", epath, 1, 2, 1024 * 1024 * 10, "info")

    -- 主线程的日志名字，是不带后缀的，比如game1就是game，和game1那个worker区分开来
    return format_log_name(node_name, node_index, true)
end


local function start_ready()
    if startup_modules and startup_modules.timer then
        Timer.stop(startup_modules.timer)
    end
    startup_modules = nil

    g_ready = true
    if LOCAL_ADDR ~= MAIN_ADDR then
        Worker.start_ready()
        printf("worker %s ready, addr = %d",
            Worker.addr_name(LOCAL_ADDR), LOCAL_ADDR)
    else
        print("main thread ready, addr =", MAIN_ADDR)
    end
    if SE then SE.fire(SE_READY) end

    -- 同步状态到集群节点，注意这里main_addr不是MAIN_ADDR而是LOCAL_ADDR
    -- main_addr是当前worker对应的线程地址
    Cluster.send_all(Cluster.set_worker_status,
        LOCAL_ADDR, LOCAL_ADDR, Worker.CLUSTER, Worker.READY)
end

local function start_next_module()
    local list = startup_modules:next()
    if not list then
        start_ready()
        return true
    end

    if not startup_modules.wait then startup_modules.wait = {} end

    local all_ready = true
    for _, func in ipairs(list) do
        if not func() then
            all_ready = false
            startup_modules.wait[func] = true
        end
    end

    if all_ready then
        return start_next_module()
    end
    return all_ready
end

local function check_module_ready()
    local wait = startup_modules.wait

    for func in pairs(wait) do
        if func(true) then
            wait[func] = nil
        end
    end

    if not next(wait) then start_next_module() end
end

-- 按优先级启动各个模块，启动完成后触发SE_READY事件
local function start_modules()
    if not startup_modules then
        return start_ready()
    end

    if start_next_module() then return end

    startup_modules.timer = Timer.interval(
        1000, 1000, -1, Rtti.temp_func(check_module_ready))
end

-- 对shutdown包一层，允许shutdown热更
local function shutdown()
    Shutdown.start()
end

-- 进程预加载必要的组件
-- @param loader 加入模块的文件或者函数
function Startup.process_init(loader)
    set_path()
    require "system.define" -- 基础定义

    local node = g_env:get("--node")

    -- game1拆分
    local node_name, node_index = string.match(node, "(%a+)(%d*)")
    node_index = assert(tonumber(node_index))

    local wtype
    for _, w in pairs(WORKER) do
        if node_name == w[2] then
            wtype = w[1]
            break
        end
    end
    assert(wtype, "no such node define")

    load_setting()
    local name = set_process_log(node_name, node_index)

    g_thread = g_mthread

    require "global.oo" -- 这个文件不能热更
    require "global.require" -- 重写require，后续用require加载的文件，都被标记为可热更
    require "global.global" -- 加载错误处理函数
    require "global.log" -- 加载log函数

    log_env_info()

    require "data.global_data"
    require "engine.engine"
    require "worker.worker"

    MAIN_ADDR = Engine.make_address(wtype, node_index, 1)

    LOCAL_TYPE = wtype
    LOCAL_ADDR = MAIN_ADDR
    LOCAL_NAME = name
    g_env:set("MAIN_ADDR", MAIN_ADDR)
    Engine.add_thread_ctx(LOCAL_ADDR, g_mthread:toludata())

    require "engine.preloader"

    Signal.mask(2, shutdown)
    Signal.mask(15, shutdown)

    math.randomseed(os.time())

    __loader = loader
    xpcall(function()
        Startup.load()
        start_modules()
    end, __G_DUMP_STACK)
end

-- 加载入口文件，将会引入所有模块
function Startup.load(reload)
    if "string" == type(__loader) then
        print("use loader: "..__loader)
        require(__loader)
    else
        __loader(reload)
    end
    Rtti.collect()
    SE.ready()
    SE.fire(SE_SCRIPT_LOADED)
end

-- 覆盖旧的loader，并加载loader
function Startup.use_loader(loader, nostart)
    __loader = loader
    if nostart then return end

    if "string" == type(__loader) then
        require(__loader)
    else
        __loader()
    end
end

-- worker预加载必要的组件
-- @param loader 加入模块的文件或者函数
function Startup.worker_init(addr, loader)
    set_path()

    require "global.oo" -- 这个文件不能热更
    require "global.require" -- 重写require，后续用require加载的文件，都被标记为可热更
    require "global.global" -- 加载错误处理函数
    require "system.define" -- 基础定义

    load_setting()

    require "data.global_data"
    require "engine.engine"
    require "worker.worker"
    require "global.log" -- 加载log函数

    local wtype, index = Engine.unmake_address(addr)
    local name = Worker.type_name(wtype)

    LOCAL_ADDR = addr
    LOCAL_TYPE = wtype
    LOCAL_NAME = string.format("%s%d", name, index)

    MAIN_ADDR = g_env:get("MAIN_ADDR")

    format_log_name(name, index)

    Engine.add_thread_ctx(addr, g_thread:toludata())

    require "engine.preloader"

    math.randomseed(os.time())

    __loader = loader
    xpcall(function()
        Startup.load()
        start_modules()
    end, __G_DUMP_STACK)
end

-- 注册按优先级启动的模块
-- @param func 需要启动的函数
-- @param priority 启动优先级，越小优先级越高，默认20
function Startup.reg(func, priority)
    if g_ready then return end -- 热更时注册过来的启动模块不用处理
    if not startup_modules then
        local PriorityManager = require "util.priority_manager"
        startup_modules = PriorityManager()
    end

    startup_modules:push(func, priority)
end
