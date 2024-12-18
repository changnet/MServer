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

WorkerHash = {} -- 以worker的addr为key，worker对象为value
WorkerSetting = {} -- 以worker的addr为ke，worker的配置为value

PROCESS_ID = 0 -- 当前进程id
LOCAL_ADDR = 0 -- 当前进程地址
LOCAL_TYPE = 0 -- 当前worker类型

local WorkerThread = require "engine.WorkerThread"

local srv_dir = g_env:get("srv_dir")

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

    -- 设置lua文件搜索路径
    package.path = srv_dir .. "/src/?.lua;"
    -- 设置c库搜索路径，用于直接加载so或者dll的lua模块
    if WINDOWS then
        package.cpath = "../c_module/?.dll;"
    else
        package.cpath = "../c_module/?.so;"
    end
end

-- 进程预加载必要的组件
function Bootstrap.process_preload(process_id)
    set_path()

    require "global.oo" -- 这个文件不能热更
    require "global.require" -- 重写require，后续用require加载的文件，都被标记为可热更
    require("global.global") -- 加载log函数

    log_env_info()

    require "engine.engine"
    require "engine.co_pool"
    require "message.thread_message"
    require "engine.signal"
    require "engine.shutdown"
    require "global.rtti"
    require "rpc.rpc"

    PROCESS_ID = process_id
    LOCAL_ADDR = Engine.make_address(PROCESS_ID, 0, 1)

    print("process start, address", LOCAL_ADDR)
    g_env:set("process_id", PROCESS_ID)

    Signal.mask(2, Shutdown.begin)
    Signal.mask(15, Shutdown.begin)

    math.randomseed(os.time())
end

-- worker预加载必要的组件
function Bootstrap.worker_preload(addr, log_name)
    set_path()

    local Log = require "engine.Log"
    Log:set_name(log_name)

    require "global.oo" -- 这个文件不能热更
    require "global.require" -- 重写require，后续用require加载的文件，都被标记为可热更
    require("global.global") -- 加载log函数

    require "engine.engine"
    require "engine.co_pool"
    require "message.thread_message"
    require "engine.signal"
    require "engine.shutdown"
    require "global.rtti"
    require "rpc.rpc"

    local proc_id, wtype = Engine.unmake_address(addr)
    assert(proc_id == tonumber(g_env:get("process_id")))

    PROCESS_ID = proc_id
    LOCAL_ADDR = addr
    LOCAL_TYPE = wtype

    math.randomseed(os.time())
end

-- 从配置文件创建一个worker
-- worker可以动态创建，但一般创建后就不删除（删除要考虑addr复用的问题）
function Bootstrap.worker_create(setting)
    local w = WorkerThread(setting.name)
    local addr = Engine.make_address(PROCESS_ID, setting.type, setting.index)

    WorkerHash[addr] = w
    WorkerSetting[addr] = setting

    w:start(srv_dir .. "/src/worker/" .. setting.file, addr)
    printf("worker %s start, addr = %d", setting.name, addr)
end

function Bootstrap.process_start(worker_setting)
    for _, s in ipairs(worker_setting) do
        Bootstrap.worker_create(s)
    end
end
