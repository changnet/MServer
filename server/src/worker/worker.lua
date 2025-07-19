-- worker通用逻辑
Worker = {}

local WorkerThread = require "engine.WorkerThread"

WorkerHash = {} -- [addr] = worker，包含所有线程的worker
WorkerSetting = {} -- [addr] = worker，只包含当前线程启动的worker
WorkerTypeName = {} -- [wtype] = wname worker类型转名字
WorkerNameType = {} -- [wname] = wtype worker名字转类型

LOCAL_ADDR = 0 -- 当前worker地址
LOCAL_TYPE = 0 -- 当前worker类型
LOCAL_NAME = "" -- 当前worker的名字

function Worker.is_all_ready()
    for addr, setting in pairs(WorkerSetting) do
        if not setting.ready then
            return addr
        end
    end
end

-- 从配置文件启动一个worker
-- worker可以动态创建，但一般创建后就不删除（删除要考虑addr复用的问题）
function Worker.start(setting)
    -- worker只能由主线程创建，否则主线程无法正确管理所有worker
    -- worker要启动另一个worker需要使用rpc调用
    assert(LOCAL_ADDR == PROCESS_ADDR)

    local name = setting.type[2]
    local w = WorkerThread(name)
    local addr = Engine.make_address(setting.type[1], setting.index)

    WorkerHash[addr] = w
    WorkerSetting[addr] = setting

    local path, err = package.searchpath(setting.file, package.path)
    if not path then error(err) end

    w:start(path, addr)
    printf("worker %s start, addr = %d", name, addr)
end

-- 从配置创建多个worker
function Worker.create(settings)
    Bootstrap.reg({
        name = "worker",
        boot = function()
            for _, s in pairs(settings) do
                Worker.start(s)
            end
        end,
        ready = function()
            local addr = Worker.is_all_ready()
            if not addr then return true end

            local name = Worker.addr_name(addr)
            return string.format("wait for %s, addr = %d", name, addr)
        end

    }, 10)
end

-- 关闭worker（此函数会阻塞直到worker线程安全退出）
function Worker.stop(addr)
    local w = WorkerHash[addr]
    if not w then
        eprint("worker stop no such worker found", addr)
        return
    end

    for other_addr in pairs(WorkerHash) do
        Call.Worker.on_stop(other_addr, addr)
    end

    w:stop(true)
    WorkerHash[addr] = nil
    printf("worker %s stop, addr = %d", Worker.addr_name(addr), addr)
end

-- 把同一进程的worker缓存到WorkerHash，加快数据交互
function Worker.on_ready(addr)
    assert(addr ~= LOCAL_ADDR)
    local w = assert(Engine.get_thread_ctx(addr))
    WorkerHash[addr] = w
end

-- worker线程收到主线程停止请求
function Worker.on_stop(addr)
    if addr == LOCAL_ADDR then
        printf("worker %s stopping, addr = %d", Worker.addr_name(addr), addr)

        g_thread:stop() -- 停止线程，但不join。join操作由主线程执行
        assert(not WorkerHash[addr])
    else
        WorkerHash[addr] = nil
    end
end

-- 获取本地local的所有地址列表
function Worker.local_addr_list()
    local list = {}
    for addr, w in pairs(WorkerHash) do
        if not w.cluster_worker then
            table.insert(list, addr)
        end
    end
end

-- 根据地址获取worker的名字，如gateway1
function Worker.addr_name(addr)
    local wtype, index = Engine.unmake_address(addr)

    local name = Worker.type_name(wtype)
    if index <= 0 then return name end

    return string.format("%s%d", name, index)
end

-- 根据类型获取名字
function Worker.type_name(wtype)
    return WorkerTypeName[wtype]
end

-- 根据名字获取类型
function Worker.name_type(name)
    return WorkerNameType[name]
end

local function init()
    for _, w in pairs(WORKER) do
        local wtype = w[1]
        local wname = w[2]
        WorkerNameType[wname] = wtype
        WorkerTypeName[wtype] = wname
    end
end

init()
