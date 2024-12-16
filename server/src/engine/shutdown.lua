-- 关服
Shutdown = {}

-- 按指定worker类型关机顺序关半
local Sequence = {WORKER_TEST}

-- 开始走关服流程
function Shutdown.begin()
    -- 根据特定的业务逻辑按顺序关闭各个worker
    for _, wt in pairs(Sequence) do
        for addr in pairs(WorkerHash) do
            local s = WorkerSetting[addr]
            if wt == s.type then
                Send.Shutdown.worker_down(addr)
                WorkerHash[addr] = nil
                printf("worker %s shutting down, addr = %d", s.name, addr)
            end
        end
    end

    -- 如果还有其他worker就是漏处理了
    for addr, w in pairs(WorkerHash) do
        print("worker no shutdown sequence found, shutting down", addr)
        --w:stop(true)
        WorkerHash[addr] = nil
    end

    --g_engine:stop()
end

function Shutdown.worker_down()
    print("worker down")
    g_worker:stop()
end

-- 终止程序(不走数据只在、清理流程)
function Shutdown.terminate()
    for _, w in pairs(WorkerHash) do
        w:stop(true)
    end
    g_engine:stop()
end
