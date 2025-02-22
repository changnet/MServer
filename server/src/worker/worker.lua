-- worker通用逻辑
Worker = {}

local WorkerThread = require "engine.WorkerThread"

WorkerHash = {} -- 以worker的addr为key，worker对象为value
WorkerSetting = {} -- 以worker的addr为key，worker的配置为value

LOCAL_ADDR = 0 -- 当前worker地址
LOCAL_TYPE = 0 -- 当前worker类型

-- 从配置文件创建一个worker
-- worker可以动态创建，但一般创建后就不删除（删除要考虑addr复用的问题）
function Worker.create(setting)
    -- worker只能由主线程创建，否则主线程无法正确管理所有worker
    -- worker要启动另一个worker需要使用rpc调用
    assert(LOCAL_ADDR == PROCESS_ADDR)

    local name = setting.type[2]
    local w = WorkerThread(name)
    local addr = Engine.make_address(setting.type[1], setting.index)

    WorkerHash[addr] = w
    WorkerSetting[addr] = setting

    local srv_dir = g_env:get("srv_dir")
    w:start(srv_dir .. "/src/worker/" .. setting.file, addr)
    printf("worker %s start, addr = %d", name, addr)
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

-- 获取worker的名字，如gateway1
function Worker.name(addr)
end

-- 根据地址获取名字
function Worker.type_name(wtype)
    for _, w in pairs(WORKER) do
        if wtype == w[1] then return w[2] end
    end
end
