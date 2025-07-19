-- 关闭进程
Shutdown = {}

-- 按指定worker类型关机顺序关闭
local Sequence = {
    WORKER.GATEWAY, WORKER.SCENE, WORKER.GAME, WORKER.PLAYER,
    WORKER.DATA, WORKER.MYSQL, WORKER.MONGODB,
}

-- 开始走关服流程
function Shutdown.process_stop()
    Cluster.close_listen()

    -- 根据特定的业务逻辑按顺序关闭各个worker
    -- 关闭时不要修改WorkerHash和WorkerSetting，rpc调用还在使用
    -- 同时避免关服中报错时，无法恢复
    for _, wt in pairs(Sequence) do
        for addr in pairs(WorkerHash) do
            local s = WorkerSetting[addr]
            if wt[1] == s.type[1] then
                printf("worker %s shutting down, addr = %d", wt[2], addr)
                Worker.stop(addr)
            end
        end
    end

    -- 一些worker不需要按业务逻辑顺序关闭，这里可以按任意顺序直接关闭了
    for addr in pairs(WorkerHash) do
        printf("worker %s shutting down, addr = %d", Worker.addr_name(addr), addr)
        Worker.stop(addr)
    end

    Cluster.close()

    g_mthread:stop()
end

-- 终止程序(不走数据保存、清理流程)
function Shutdown.terminate()
    for _, w in pairs(WorkerHash) do
        w:stop(true)
    end
    g_mthread:stop()
end
