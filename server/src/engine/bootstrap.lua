-- 启动
Bootstrap = {}

local WorkerThread = require "engine.WorkerThread"

-- lua加载文件可以自动识别/和\，所以这里统一用linux的就行
local base_path = g_env:get("srv_dir") .. "/src/worker/"

-- 从配置文件创建一个worker
-- worker可以动态创建，但一般创建后就不删除（删除要考虑addr复用的问题）
function Bootstrap.worker_create(setting)
    local w = WorkerThread(setting.name)
    local addr = Engine.make_address(PROCESS_ID, setting.type, setting.index)

    WorkerHash[addr] = w
    WorkerSetting[addr] = setting

    w:start(base_path .. setting.file)
    printf("worker %s start, addr = %d", setting.name, addr)
end

function Bootstrap.process_start(worker_setting)
    for _, s in ipairs(worker_setting) do
        Bootstrap.worker_create(s)
    end
end
