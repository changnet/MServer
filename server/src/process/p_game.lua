-- 测试进程入口文件(此文件不热更)

Bootstrap.process_init()

-- 当前进程需要启动的worker {线程名, 入口文件名}
-- 线程名不能超过16个字段
local worker_setting = {
    {file = "w_game.lua", type = WORKER.GAME, index = 1},
    {file = "w_player.lua", type = WORKER.PLAYER, index = 1},
    {file = "w_player.lua", type = WORKER.PLAYER, index = 2},
    -- {file = "w_scene.lua", type = WORKER.SCENE, index = 1},
    -- {file = "w_scene.lua", type = WORKER.SCENE, index = 2},
}

Worker.start(worker_setting)
Cluster.connect(g_setting.cluster, "gateway1")
-- Cluster.connect("data", 1, 1)
-- Bootstrap.start()
