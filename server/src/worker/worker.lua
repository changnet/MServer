-- worker通用逻辑
Worker = {}

local WorkerThread = require "engine.WorkerThread"

WorkerHash = WorkerHash or {} -- [addr] = worker，包含集群节点，不包含主线程、自己及非当前线程的集群节点
WorkerData = WorkerData or {} -- [addr] = {}，所有worker的数据，包含集群节点，不包含主主线程、自己
WorkerTypeName = {} -- [wtype] = wname worker类型转名字
WorkerNameType = {} -- [wname] = wtype worker名字转类型

LOCAL_ADDR = LOCAL_ADDR or 0 -- 当前worker地址
LOCAL_TYPE = LOCAL_TYPE or 0 -- 当前worker类型
LOCAL_NAME = LOCAL_NAME or "" -- 当前worker的名字
MAIN_ADDR = MAIN_ADDR or 0 -- 当前主线程对应的worker地址

-- 本地启动的worker是否都已启动完成
-- @return 返回未启动完成的worker地址
function Worker.is_local_ready()
    for addr, data in pairs(WorkerData) do
        if not data.node_type and not data.ready then
            return addr
        end
    end
end

-- 获取worker在WorkerData中的数据，如果不存在则创建
function Worker.get_data(addr)
    local data = WorkerData[addr]
    if not data then
        data = {}
        WorkerData[addr] = data
    end

    return data
end

-- 从配置文件启动一个worker
-- worker可以动态创建，但一般创建后就不删除（删除要考虑addr复用的问题）
function Worker.start(setting)
    -- worker只能由主线程创建，否则主线程无法正确管理所有worker
    -- worker要启动另一个worker需要使用rpc调用
    assert(Engine.is_main_addr(LOCAL_ADDR))

    local name = setting.type[2]
    local w = WorkerThread(name)
    local addr = Engine.make_address(setting.type[1], setting.index)

    WorkerHash[addr] = w
    WorkerData[addr] = table.copy(setting)

    local path, err = package.searchpath(setting.file, package.path)
    if not path then error(err) end

    w:start(path, addr)
    printf("worker %s start, addr = %d", name, addr)
end

-- 从配置创建多个worker
function Worker.start_later(settings)
    Bootstrap.reg({
        name = "worker",
        boot = function()
            for _, s in pairs(settings) do
                Worker.start(s)
            end
        end,
        ready = function()
            local addr = Worker.is_local_ready()
            if not addr then return true end

            local name = Worker.addr_name(addr)
            return false, string.format("wait for %s, addr = %d", name, addr)
        end

    }, 10)
end

-- 关闭worker（此函数会阻塞直到worker线程安全退出）
function Worker.stop(addr)
    assert(LOCAL_ADDR == MAIN_ADDR) -- 仅允许主线程操作worker关闭

    local w = WorkerHash[addr]
    if not w then
        eprint("worker stop no such worker found", addr)
        return
    end

    printf("worker %s shutting down, addr = %d", Worker.addr_name(addr), addr)
    -- 先从其他worker移除
    -- Worker.call_all(Worker.on_set_status, addr, nil, 0)

    -- 自己再关闭
    Call.Worker.on_stop(addr, addr)

    -- 最后由主线程移除worker记录
    w:stop(true)
    WorkerHash[addr] = nil
    WorkerData[addr] = nil
    printf("worker %s stop, addr = %d", Worker.addr_name(addr), addr)
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

-- 标记某个worker状态，并同步到其他worker
-- @param status 0停止 1启动 2启动完成
function Worker.on_set_status(addr, node_type, status)
    if 0 == status then
        if addr == LOCAL_ADDR then
            printf("worker %s stopping, addr = %d", Worker.addr_name(addr), addr)

            g_thread:stop() -- 停止线程，但不join。join操作由主线程执行
            assert(not WorkerHash[addr])
        else
            WorkerHash[addr] = nil
        end
        WorkerData[addr] = nil
        SE.fire(SE_WORKER_STOP, addr, node_type)
    elseif 1 == status then
        SE.fire(SE_WORKER_START, addr, node_type)
    elseif 2 == status then
        assert(addr ~= LOCAL_ADDR)

        -- 主线程负责启动worker，一开始就设置了worker的，不要覆盖
        -- 主线程最原始的worker被释放掉是会触发gc的，程序会当掉
        if LOCAL_ADDR ~= MAIN_ADDR then
            local w = assert(Engine.get_thread_ctx(addr))
            WorkerHash[addr] = w
        end

        -- 如果没有这个配置，那说明当前worker不关注这个addr的状态
        local data = Worker.get_data(addr)
        data.ready = 1
    end
end

-- 标记某个worker状态，并同步到其他worker
-- @param status 0停止 1启动 2启动完成
function Worker.set_status(addr, node_type, status)
    -- 主线程不走这个启动流程
    assert(LOCAL_ADDR == MAIN_ADDR)

    -- 自己本身是不触发事件的
    -- Worker.on_set_status(addr, node_type, status)

    -- 同步到其他worker，包括所有本地的worker和直连的worker
    Worker.send_all(Worker.on_set_status, addr, node_type, status)
end

-- 以广播方式对所有worker发起rpc send调用，包括主线程，不包括自己
function Worker.send_all(func, ...)
    if LOCAL_ADDR ~= MAIN_ADDR then Send.invoke(MAIN_ADDR, func, ...) end
    for addr in pairs(WorkerData) do
        Send.invoke(addr, func, ...)
    end
end

-- 以广播方式对所有worker发起rpc call调用，包括主线程，不包括自己
function Worker.call_all(func, ...)
    if LOCAL_ADDR ~= MAIN_ADDR then Send.invoke(MAIN_ADDR, func, ...) end
    for addr in pairs(WorkerData) do
        Send.invoke(addr, func, ...)
    end
end

-- 获取本进程可以转发的worker列表
function Worker.get_forward_addr_list()
    local forward_list = {}
    for addr, data in pairs(WorkerData) do
        local nt = data.node_type
        if nt == Cluster.NODE_PROCESS or nt == Cluster.NODE_WORKER then
            -- 只中转和自己直连的节点。不能A-B-C-D这样多层中转
            table.insert(forward_list, addr)
        end
    end

    return forward_list
end

-- 获取本地local的所有地址列表
function Worker.get_local_addr_list()
    local addr_list = {}
    for addr, data in pairs(WorkerData) do
        if not data.node_type  then table.insert(addr_list, addr) end
    end

    return addr_list
end

-- 根据地址获取worker的名字，包含索引，如gateway1
function Worker.addr_name(addr)
    local wtype, index, main = Engine.unmake_address(addr)

    local name = Worker.type_name(wtype)
    if index <= 0 or main then return name end

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
