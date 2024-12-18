-- 关服
Shutdown = {}

-- 按指定worker类型关机顺序关半
local Sequence = {WORKER_TEST}

-- 开始走关服流程
function Shutdown.begin()
    local is_shut = {}

    -- 根据特定的业务逻辑按顺序关闭各个worker
    -- 关闭时不要修改WorkerHash和WorkerSetting，rpc调用还在使用
    -- 同时避免关服中报错时，无法恢复
    for _, wt in pairs(Sequence) do
        for addr, w in pairs(WorkerHash) do
            local s = WorkerSetting[addr]
            if not w:is_start() then
                -- 已经被关闭或者未开启成功
                printf("worker %s not start, addr = %d", s.name, addr)
            elseif wt == s.type then
                printf("worker %s shutting down, addr = %d", s.name, addr)

                is_shut[addr] = true
                Call.Shutdown.worker_stop(addr)
            end
        end
    end

    for addr, w in pairs(WorkerHash) do
        if not is_shut[addr] and w:is_start() then
            -- 如果还有其他worker就是漏处理了
            print("worker no shutdown sequence found, shutting down", addr)
        end

        -- 上面已通知所有worker关闭，这里join所有worker等待worker线程处理完成
        w:stop(true)
    end

    g_engine:stop()
end

function Shutdown.worker_stop()
    print("worker stop now", LOCAL_ADDR)
    g_worker:stop()
end

-- 终止程序(不走数据只在、清理流程)
function Shutdown.terminate()
    for _, w in pairs(WorkerHash) do
        w:stop(true)
    end
    g_engine:stop()
end
