-- 测试进程入口文件(此文件不热更)

Bootstrap.process_init("modules.process_loader")

if g_setting.mode == "cluster" then
    -- 集群模式，一个进程只启动一个类型的worker
    Cluster.start_later(g_setting.cluster, "game")
elseif g_setting.mode == "thread" then
    -- 多线程模式，所有worker在同一进程，不需要监听和连接
    Worker.start_later({
        {file = "worker.w_game", type = WORKER.GAME, index = 1},
        {file = "worker.w_player", type = WORKER.PLAYER, index = 1},
        {file = "worker.w_player", type = WORKER.PLAYER, index = 2},
        -- {file = "w_scene.lua", type = WORKER.SCENE, index = 1},
        -- {file = "w_scene.lua", type = WORKER.SCENE, index = 2},
    })
elseif g_setting.mode == "process" then
    -- 进程模式，worker在多个进程
    Worker.start_later({
        {file = "worker.w_game", type = WORKER.GAME, index = 1},
        {file = "worker.w_player", type = WORKER.PLAYER, index = 1},
        {file = "worker.w_player", type = WORKER.PLAYER, index = 2},
        -- {file = "w_scene.lua", type = WORKER.SCENE, index = 1},
        -- {file = "w_scene.lua", type = WORKER.SCENE, index = 2},
    })
    Cluster.listen_later(g_setting.cluster, "game")
end
