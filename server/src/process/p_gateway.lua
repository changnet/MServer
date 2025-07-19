-- 测试进程入口文件(此文件不热更)

Bootstrap.process_init("modules.process_loader")

-- 当前进程需要启动的worker {线程名, 入口文件名}
local worker_setting = {
    {file = "worker.w_gateway", type = WORKER.GATEWAY, index = 1},
}

Worker.create(worker_setting)

-- 监听game进程的连接
Cluster.listen(g_setting.cluster, g_env:get("--node"), true)
-- Cluster.connect("data", 1, 1)
