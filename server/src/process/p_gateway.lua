-- 测试进程入口文件(此文件不热更)

Bootstrap.process_init("modules.process_loader")

-- 当前进程需要启动的worker {线程名, 入口文件名}
local worker_setting = {
    {file = "worker.w_gateway", type = WORKER.GATEWAY, index = 1},
}

Worker.start_later(worker_setting)

-- 连接game，不要让game来连接gateway。因为在gateway开启一个端口监听内部连接是很危险
Cluster.connect_later({{g_setting.cluster, "game1"}})

-- 使用process连接模式，则需要连接data节点
Cluster.connect_later({{g_setting.cluster, "data1"}})

-- 使用中转模式，则等待data节点启动完成
-- Cluster.wait_ready({"data1"})
