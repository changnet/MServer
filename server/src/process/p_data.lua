-- 测试进程入口文件(此文件不热更)

Bootstrap.process_init("engine.preloader")

-- 用了data进程，只能以多进程方式启动。集群只能用mysql或者mongodb一个worker，thread模式不用data进程
assert(g_setting.mode == "process")

-- 当前进程需要启动的worker {线程名, 入口文件名}
local worker_setting = {
    {file = "data.w_data", type = WORKER.DATA, index = 1},
    {file = "data.w_mysql", type = WORKER.MYSQL, index = 1},
    {file = "data.w_mongodb", type = WORKER.MONGODB, index = 1},
}

Worker.start_later(worker_setting)
Cluster.connect_later({{g_setting.cluster, "game"}})
