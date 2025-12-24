-- 关闭进程
Shutdown = {}

-- 需要按优先级关闭的模块
local this = memory("Shutdown")

g_shuttingdown = nil -- 是否开启关服

-- 终止程序(不走数据保存、清理流程)
function Shutdown.terminate()
    for _, w in pairs(WorkerHash) do
        if not w.cluster_worker then
            w:stop(true)
        end
    end
    g_mthread:stop()
end

-- 执行最终关闭清理程序
local function shutdown_complete()
    if LOCAL_ADDR == MAIN_ADDR then return end

    -- socket模块是异步的，要等backend线程处理完再退出，懒得去检测socket_mgr那边的状态
    Engine.sleep(100)

    g_mthread:stop()
end

-- 按注册的优先级关闭模块
function Shutdown.start()
    g_shuttingdown = true
    if not this.shutdown_modules then
        shutdown_complete()
        return
    end

    -- 关闭函数不允许失败，不能用异步回调，可以用协程异步，但最终必须成功
    local list = this.shutdown_modules:next()
    while list do
        for _, mod in ipairs(list) do
            mod.func()
        end

        list = this.shutdown_modules:next()
    end

    shutdown_complete()
end

-- 注册按优先级关闭的模块
-- @param mod 需要关闭的模块，包括starty函数
-- @param priority 启动优先级，越小优先级越高，默认20
function Shutdown.reg(mod, priority)
    --[[
    和startup不一样，startup在启动过程中不会热更，启动完成后startup就没用了
    shutdown在启动过程中可以热更，因此要考虑一些模块重复注册，或者覆盖的问题

    很多模块（比如cluster）虽然会引用，但需要启动才需要关闭，所以会有重新添加的问题
    mod.name作为唯一标识符，如果存在则覆盖，否则新增
    ]]
    if not this.shutdown_modules then
        local PriorityManager = require "util.priority_manager"
        this.shutdown_modules = PriorityManager()

        this.shutdown_modules.mods = {}
    end
    mod.priority = priority or 20

    local old = this.shutdown_modules.mods[mod.name]
    if old then
        if old.priority == mod.priority then return end
        this.shutdown_modules:remove(old)
    end

    this.shutdown_modules:push(mod, priority)
end
