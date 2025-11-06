-- worker通用逻辑
Worker = {
    STOP     = 0, -- 关闭
    STARTING = 1, -- 启动中
    READY    = 2, -- 启动完成
}

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

    local data = table.copy(setting)
    data.status = Worker.STARTING
    data.node_type = Cluster.NODE_LOCAL

    WorkerHash[addr] = w
    WorkerData[addr] = data

    local path, err = package.searchpath(setting.file, package.path)
    if not path then error(err) end

    w:start(path, addr)
    printf("worker %s starting, addr = %d", name, addr)
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

-- 主线程收到worker启动完成
function Worker.on_start_ready(addr)
    -- 当前worker启动完成，先通知主线程，再由主线程同步到其他worker
    -- 因为启动的时候当前worker是没有其他worker的数据，所以这时候只能由主线程同步

    Worker.set_status(addr, Cluster.NODE_LOCAL, Worker.READY)

    for other_addr, data in pairs(WorkerData) do
        if other_addr ~= addr and Cluster.NODE_LOCAL == data.node_type then
            -- 同步给其他local worker
            Send.Worker.set_status(
                other_addr, addr, Cluster.NODE_LOCAL, Worker.READY)
        end
    end
end

-- worker启动完成
function Worker.start_ready()
    assert(LOCAL_ADDR ~= MAIN_ADDR)

    -- 通知主线程启动完成
    Send.Worker.on_start_ready(MAIN_ADDR, LOCAL_ADDR)
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
    -- 先通知其他worker，这个worker需要关闭
    for other_addr, data in pairs(WorkerData) do
        if other_addr ~= addr and Cluster.NODE_LOCAL == data.node_type then
            Send.Worker.set_status(other_addr, addr, Cluster.NODE_LOCAL, Worker.STOP)
        end
    end

    -- 通知对应的线程自己关闭
    Call.Worker.stopping(addr, addr)

    -- 最后由主线程移除worker记录
    w:stop(true)
    WorkerHash[addr] = nil
    WorkerData[addr] = nil
    printf("worker %s stop, addr = %d", Worker.addr_name(addr), addr)
end

-- worker线程收到主线程停止请求
function Worker.stopping(addr)
    assert(addr == LOCAL_ADDR)
    printf("worker %s stopping, addr = %d", Worker.addr_name(addr), addr)

    -- 执行关闭流程，依次关闭各个模块
    Shutdown.start()

    g_thread:stop() -- 停止线程，但不join。join操作由主线程执行
    assert(not WorkerHash[addr])
    assert(not WorkerData[addr])
end

-- 标记某个worker状态，并同步到其他worker
-- @param status 0停止 1启动 2启动完成
function Worker.set_status(addr, node_type, status)
    assert(addr ~= LOCAL_ADDR)

    if Worker.STOP == status then
        WorkerHash[addr] = nil
        WorkerData[addr] = nil
        SE.fire(SE_WORKER_STOP, addr, node_type)
    elseif Worker.STARTING == status then
        local data = Worker.get_data(addr)
        data.status = status
        SE.fire(SE_WORKER_START, addr, node_type)
    elseif Worker.READY == status then
        -- 主线程负责启动worker，一开始就设置了WorkerHash[addr]，不要覆盖
        -- 主线程最原始的worker被释放掉是会触发gc的，程序会当掉
        if LOCAL_ADDR ~= MAIN_ADDR and node_type == Cluster.NODE_LOCAL then
            local w = assert(Engine.get_thread_ctx(addr))
            WorkerHash[addr] = w
        end

        local data = Worker.get_data(addr)
        data.status = status
    else
        assert(false)
    end
end

-- 以广播方式对所有worker发起rpc send调用，包括主线程，不包括自己
function Worker.send_other(func, ...)
    if LOCAL_ADDR ~= MAIN_ADDR then Send.invoke(MAIN_ADDR, func, ...) end
    for addr in pairs(WorkerData) do
        Send.invoke(addr, func, ...)
    end
end

-- 以广播方式对所有worker发起rpc call调用，包括主线程，不包括自己
function Worker.call_other(func, ...)
    if LOCAL_ADDR ~= MAIN_ADDR then Send.invoke(MAIN_ADDR, func, ...) end
    for addr in pairs(WorkerData) do
        Send.invoke(addr, func, ...)
    end
end

-- 获取需要同步的worker状态列表
function Worker.get_status_list()
    local local_send_type = Engine.is_main_addr(LOCAL_ADDR)
        and Cluster.NODE_PROCESS or Cluster.NODE_WORKER

    local status_list = {}
    for addr, data in pairs(WorkerData) do
        local node_type = data.node_type

        local send_type
        if node_type == Cluster.NODE_LOCAL then
            send_type = local_send_type
        elseif node_type == Cluster.NODE_PROCESS then
            send_type = Cluster.NODE_FORWARD
        -- worker与worker之间直连不走转发逻辑，就不提供转发的节点了
        -- 数据不能多次中转，NODE_FORWARD节点也不同步
        -- elseif node_type == Cluster.NODE_WORKER then
        --     send_type = Cluster.NODE_FORWARD
        end

        table.insert(status_list, {
            addr = addr,
            status = data.status,
            node_type = send_type,
        })
    end

    return status_list
end

-- 根据地址获取worker的名字，包含索引，如gateway1
function Worker.addr_name(addr)
    -- TODO 这个函数不常用，因此不做缓存
    local wtype, index, main = Engine.unmake_address(addr)

    local name = Worker.type_name(wtype)
    if main then
        return string.format("%s_%d", name, index)
    else
        return string.format("%s%d", name, index)
    end
end

-- 根据名字获取地址
-- @param fullname 名字，必须带数据，如gateway1
function Worker.name_addr(fullname)
    -- TODO 这个函数不常用，因此不做缓存

    local name, main, index = string.match(fullname, "^(%w+)(_?)(%d*)$")

    index = tonumber(index)
    main = main == "_" and 1 or 0
    local wtype = WorkerNameType[name]
    return Engine.make_address(wtype, index, main)
end

-- 根据类型获取名字
function Worker.type_name(wtype)
    return WorkerTypeName[wtype]
end

-- 根据名字获取类型
-- @param name 名字，不带数据，比如gateway
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
