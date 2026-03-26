-- worker通用逻辑
Worker = {
    STOP     = 0, -- 关闭
    STARTING = 1, -- 启动中
    READY    = 2, -- 启动完成

    THREAD  = 1, -- worker模式，本地线程
    CLUSTER = 2, -- worker模式，集群节点，socket通信
    PROXY   = 3, -- worker模式，需要代理转发的集群节点，最慢
}

local WorkerThread = require "engine.WorkerThread"

WorkerHash = WorkerHash or {} -- [addr] = worker，包含集群节点，不包含主线程、自己及非当前线程的集群节点
WorkerData = WorkerData or {} -- [addr] = {}，所有worker的数据，包含集群节点，不包含主线程、自己
WorkerNameType = {} -- [wname] = wtype worker名字转类型

LOCAL_ADDR = LOCAL_ADDR or 0 -- 当前worker地址
LOCAL_TYPE = LOCAL_TYPE or 0 -- 当前worker类型
LOCAL_NAME = LOCAL_NAME or "" -- 当前worker的名字
MAIN_ADDR = MAIN_ADDR or 0 -- 当前主线程对应的worker地址

local this = memory("Worker")

-- 所有本地启动的worker是否都已启动完成
-- @return 返回未启动完成的worker地址
function Worker.which_local_not_ready()
    for addr, data in pairs(WorkerData) do
        if Worker.THREAD == data.mode and Worker.READY ~= data.status then
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

local function shutdown()
    -- 根据各worker定义的 `sequ` 值来决定关闭顺序，值越大越先关闭
    local seq_map = {}
    for _, w in pairs(WORKER) do
        local sequ = w.sequ or 0
        seq_map[sequ] = seq_map[sequ] or {}
        table.insert(seq_map[sequ], w.type)
    end

    local seq_list = {}
    for sequ, list in pairs(seq_map) do
        table.insert(seq_list, {sequ = sequ, list = list})
    end

    table.sort(seq_list, function(a, b) return a.sequ > b.sequ end)

    for _, item in ipairs(seq_list) do
        for _, wt in ipairs(item.list) do
            for addr, w in pairs(WorkerHash) do
                local d = WorkerData[addr]
                if not w.cluster_worker and wt == d.type then
                    Worker.stop(addr)
                end
            end
        end
    end

    -- 关闭剩余的本地worker（不按特定顺序）
    for addr, w in pairs(WorkerHash) do
        if not w.cluster_worker then
            Worker.stop(addr)
        end
    end
end

-- 从配置文件启动一个worker
-- worker可以动态创建，但一般创建后就不删除（删除要考虑addr复用的问题）
function Worker.start(setting)
    -- worker只能由主线程创建，否则主线程无法正确管理所有worker
    -- worker要启动另一个worker需要使用rpc调用
    assert(Engine.is_main_addr(LOCAL_ADDR))

    local wtype = setting.type
    local name = Worker.type_name(wtype)
    local w = WorkerThread(name)
    local addr = Engine.make_address(wtype, setting.index)

    local data = table.copy(setting)
    data.status = Worker.STARTING
    data.mode = Worker.THREAD

    WorkerHash[addr] = w
    WorkerData[addr] = data

    local path, err = package.searchpath(setting.file, package.path)
    if not path then error(err) end

    w:start(path, addr)
    printf("worker %s starting, addr = %d", name, addr)

    -- 同步状态到远程集群节点
    Cluster.send_all(Cluster.set_worker_status,
        LOCAL_ADDR, addr, Worker.CLUSTER, Worker.STARTING)
end

local function sort_start_sequence(settings)
    local list = {}
    local hash = {}
    for _, s in ipairs(settings) do
        local sequ = WORKER[s.type].sequ
        if not hash[sequ] then
            hash[sequ] = {list = {}, sequ = sequ}
            table.insert(list, hash[sequ])
        end
        table.insert(hash[sequ].list, s)
    end

    table.sort(list, function(a, b) return a.sequ < b.sequ end)

    return list
end

local function start_next_sequence_worker()
    local start_index = this.start_index
    if not start_index then return end

    -- 上一个优先级的worker尚未启动完成
    if Worker.which_local_not_ready() then return end

    local index = this.start_index + 1
    local start_list = this.start_list[index]
    if not start_list then
        this.start_list = nil
        this.start_index = nil
        return false
    end

    this.start_index = index
    for _, s in pairs(start_list.list) do
        Worker.start(s)
    end
end

-- 从配置创建多个worker
function Worker.start_later(settings)
    this.start_list = sort_start_sequence(settings)
    Startup.reg(function(retry)
        if not retry then
            Shutdown.reg({
                name = "worker",
                func = shutdown,
            }, 64)
            this.start_index = 0
            start_next_sequence_worker()
        end
        local addr = Worker.which_local_not_ready()
        if not addr then
            -- 还存在未启动的，这应该是有问题了
            if this.start_index then
                print("wait for worker start next")
                return false
            end
            return true
        end

        local name = Worker.addr_name(addr)
        printf("wait for %s, addr = %d", name, addr)
        return false
    end, 10)
end

-- 主线程收到worker启动完成
function Worker.on_start_ready(addr)
    -- 一个worker启动完成，先通知主线程，再由主线程同步到其他worker
    -- 因为启动的时候当前worker是没有其他worker的数据，所以这时候只能由主线程同步
    assert(MAIN_ADDR == LOCAL_ADDR)

    Worker.set_status(MAIN_ADDR, addr, Worker.THREAD, Worker.READY)

    for other_addr, data in pairs(WorkerData) do
        if other_addr ~= addr and Worker.THREAD == data.mode then
            -- 同步给其他local worker
            Send.Worker.set_status(other_addr,
                MAIN_ADDR, addr, Worker.THREAD, Worker.READY)
        end
    end

    Cluster.send_all(Cluster.set_worker_status,
        LOCAL_ADDR, addr, Worker.CLUSTER, Worker.READY)

    start_next_sequence_worker()
end

-- worker全局定时器
local function do_worker_timer()
    local now = Engine.time()

    SE.fire(SE_SEC_TIMER, now)

    local next_min = this.next_min
    if now > next_min then
        -- this.next_min = next_min + 60 在服务器卡的时候无法修正为下一分钟
        -- 这个定时器要保证整分钟触发
        this.next_min = time.get_next_minite(now)
        SE.fire(SE_MIN_TIMER, now)
    end
end

-- 非主线程worker启动完成
function Worker.start_ready()
    assert(LOCAL_ADDR ~= MAIN_ADDR)

    -- 通知主线程启动完成
    Send.Worker.on_start_ready(MAIN_ADDR, LOCAL_ADDR)

    -- 触发其他worker启动完成事件
    for addr, data in pairs(WorkerData) do
        if data.status == Worker.READY then
            SE.fire(SE_WORKER_BOTH_READY, addr, data.mode)
        end
    end

    this.next_min = time.get_next_minite()

    -- 启动worker全局定时器
    Timer.interval(1000, 1000, -1, do_worker_timer)
end

-- 关闭worker（此函数会阻塞直到worker线程安全退出）
function Worker.stop(addr)
    assert(LOCAL_ADDR == MAIN_ADDR) -- 仅允许主线程操作worker关闭

    local w = WorkerHash[addr]
    if not w then
        eprint("worker stop no such worker found", addr)
        return
    end

    -- 先通知其他worker，这个worker需要关闭
    for other_addr, data in pairs(WorkerData) do
        if other_addr ~= addr and Worker.THREAD == data.mode then
            Send.Worker.set_status(other_addr,
                MAIN_ADDR, addr, Worker.THREAD, Worker.STOP)
        end
    end

    -- 通知对应的线程自己关闭
    Call.Worker.stopping(addr, addr)

    -- 最后由主线程移除worker记录
    w:stop(true)
    WorkerHash[addr] = nil
    WorkerData[addr] = nil
    printf("worker shutdown %s, addr = %d", Worker.addr_name(addr), addr)
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
-- @param src_addr 来源地址。A连接B进程，B包含鑫个节点，则这些节点的src_addr都是B进程地址
-- @param status 0停止 1启动 2启动完成
function Worker.set_status(src_addr, addr, mode, status)
    assert(addr ~= LOCAL_ADDR)

    if Worker.STOP == status then
        WorkerHash[addr] = nil
        WorkerData[addr] = nil
        SE.fire(SE_WORKER_STOP, addr, mode)
    elseif Worker.STARTING == status then
        local data = Worker.get_data(addr)
        local old_status = data.status
        data.status = status
        data.mode = mode
        data.src_addr = src_addr

        -- 对于集群节点，交叉和转发会导致同步多次，同一个状态只触发一次
        if old_status ~= status then
            SE.fire(SE_WORKER_START, addr, mode)
        end
    elseif Worker.READY == status then
        -- 主线程负责启动worker，一开始就设置了WorkerHash[addr]，不要覆盖
        -- 主线程最原始的worker被释放掉是会触发gc的，程序会当掉
        if LOCAL_ADDR ~= MAIN_ADDR and mode == Worker.THREAD then
            local w = assert(Engine.get_thread_ctx(addr))
            WorkerHash[addr] = w
        end

        local data = Worker.get_data(addr)
        local old_status = data.status

        data.status = status
        data.mode = mode
        data.src_addr = src_addr
        if old_status ~= status then
            SE.fire(SE_WORKER_OTHER_READY, addr, mode)
            if g_ready then SE.fire(SE_WORKER_BOTH_READY, addr, mode) end
        end
    else
        assert(false)
    end
end

-- 以广播方式对所有worker发起rpc send调用，包括主线程，不包括自己
function Worker.send_other_local(func, ...)
    if LOCAL_ADDR ~= MAIN_ADDR then Send.invoke(MAIN_ADDR, func, ...) end
    for addr, data in pairs(WorkerData) do
        if data.mode == Worker.THREAD then
            Send.invoke(addr, func, ...)
        end
    end
end

-- 以广播方式对所有worker发起rpc call调用，包括主线程，不包括自己
function Worker.call_other_local(func, ...)
    if LOCAL_ADDR ~= MAIN_ADDR then Call.invoke(MAIN_ADDR, func, ...) end
    for addr, data in pairs(WorkerData) do
        if data.mode == Worker.THREAD then
            Call.invoke(addr, func, ...)
        end
    end
end

-- 以广播方式对所有worker发起rpc send调用，包括集群节点，不包括自己
function Worker.send_other_type(wtype, func, ...)
    for addr, data in pairs(WorkerData) do
        if data.type == wtype then
            Send.invoke(addr, func, ...)
        end
    end
end

-- 获取需要同步的worker状态列表
function Worker.get_status_list()
    local local_status = g_ready and Worker.READY or Worker.STARTING
    local status_list = {{
        addr = LOCAL_ADDR,
        status = local_status,
        mode = Worker.CLUSTER
    }}
    for addr, data in pairs(WorkerData) do
        local mode = data.mode

        if mode == Worker.THREAD then
            table.insert(status_list, {
                addr = addr,
                status = data.status,
                mode = Worker.CLUSTER,
            })
        end
    end

    return status_list
end

-- 从主线程同步已有worker的状态
function Worker.sync_status_from_main()
    local MAIN_ADDR = MAIN_ADDR
    local LOCAL_ADDR = LOCAL_ADDR
    assert(LOCAL_ADDR ~= MAIN_ADDR)

    local list = Call.Worker.get_status_list(MAIN_ADDR)
    for _, s in pairs(list) do
        local addr = s.addr
        if addr ~= LOCAL_ADDR and addr ~= MAIN_ADDR then
            Worker.set_status(MAIN_ADDR, addr, s.mode, s.status)
        end
    end
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

    local name, main, index = string.match(fullname, "^([%a]+)(_?)(%d*)$")

    index = tonumber(index)
    main = main == "_" and 1 or 0
    local wtype = WorkerNameType[name]
    return Engine.make_address(wtype, index, main)
end

-- 根据类型获取名字
function Worker.type_name(wtype)
    return WORKER[wtype].name
end

-- 根据名字获取类型
-- @param name 名字，不带数据，比如gateway
function Worker.name_type(name)
    return WorkerNameType[name]
end

local function init()
    for _, w in pairs(WORKER) do
        local wtype = w.type
        local wname = w.name
        WorkerNameType[wname] = wtype
    end
    Rtti.name_func("Worker.do_worker_timer", do_worker_timer)
end

init()
