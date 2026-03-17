-- 测试进程入口文件(此文件不热更)

Startup.process_init(function()
    Startup.use_loader("modules.process_loader")

    if g_setting.mode == "cluster" then
        -- 集群模式，一个进程只启动一个类型的worker
        Cluster.start_later(g_setting.cluster, "game_1")
    elseif g_setting.mode == "thread" then
        -- 多线程模式，所有worker在同一进程，不需要监听和连接
        Worker.start_later({
            {file = "worker.w_game", type = W.GAME, index = 1},
            {file = "worker.w_player", type = W.PLAYER, index = 1},
            {file = "worker.w_player", type = W.PLAYER, index = 2},
            -- {file = "w_scene.lua", type = W.SCENE, index = 1},
            -- {file = "w_scene.lua", type = W.SCENE, index = 2},
            {file = "worker.w_account", type = W.ACCOUNT, index = 1},
            {file = "worker.w_gateway", type = W.GATEWAY, index = 1},
            {file = "data.w_data", type = W.DATA, index = 1},
            -- {file = "data.w_mysql", type = W.MYSQL, index = 1},
            {file = "data.w_mongodb", type = W.MONGODB, index = 1},
            {file = "log.w_log", type = W.LOG, index = 1},
        })
    elseif g_setting.mode == "process" then
        -- 进程模式，worker在多个进程
        Worker.start_later({
            {file = "worker.w_game", type = W.GAME, index = 1},
            {file = "worker.w_player", type = W.PLAYER, index = 1},
            {file = "worker.w_player", type = W.PLAYER, index = 2},
            -- {file = "w_scene.lua", type = W.SCENE, index = 1},
            -- {file = "w_scene.lua", type = W.SCENE, index = 2},
            {file = "log.w_log", type = W.LOG, index = 1},
        })
        Cluster.listen_later(g_setting.cluster, "game_1")
    end
end)
