-- 测试进程入口文件(此文件不热更)

Bootstrap.process_init("modules.process_loader")

if g_setting.mode == "cluster" then
    -- 集群模式，一个进程只启动一个类型的worker
    Cluster.start_later(g_setting.cluster)
else
    -- 当前进程需要启动的worker {线程名, 入口文件名}
    -- 线程名不能超过16个字段
    local worker_setting = {
        {file = "worker.w_game", type = WORKER.GAME, index = 1},
        {file = "worker.w_player", type = WORKER.PLAYER, index = 1},
        {file = "worker.w_player", type = WORKER.PLAYER, index = 2},
        -- {file = "w_scene.lua", type = WORKER.SCENE, index = 1},
        -- {file = "w_scene.lua", type = WORKER.SCENE, index = 2},
    }

    if g_setting.mode == "thread" then
        Worker.start_later(worker_setting)
    else
        Worker.start_later(worker_setting)
        Cluster.listen_later(g_setting.cluster, "game")
    end
end
