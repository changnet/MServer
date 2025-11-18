-- 服务器 配置 样本
-- 任何配置都要写到配置样本，并上传版本管理(svn、git...)
-- 各个服部署时配置从样本修改而来，但不要上传。避免配置被覆盖

return
{
    mongodb =
    {
        ip = "127.0.0.1", -- mongodb ip
        port = 27017, -- mongodb 端口
        db = "test_999", -- 需要连接的数据库
        user = "test", -- mongo 用户(以后弄个加密，以免明文保存)
        pwd  = "test", -- mongo 密码(以后弄个加密，以免明文保存)
    },

    mysql =
    {
        ip   = "127.0.0.1";
        port = 3306,
        user = "test",
        pwd  = "test",
        db   = "test_999",
    },

    gm = true, -- 是否启用gm
    cli = true, -- 是否启用命令行输入
    lang = "zh", -- 简体中文
    server = 1, -- 当前服务器id
    mode = "process", -- 当前进程启动模式

    -- 网关配置（监听客户端连接的ip和端口）
    gateway =
    {
        -- 在virtualbox的端口转发模式下，127.0.0.1转发不成功
        gateway1 = {host = "::", port = 10001}
        -- 一个服有多个网关的话依次列出
        -- gateway2 = {host = "::", port = 10002}
    },

    -- 集群
    cluster =
    {
        game_1 = {ip = "::1", port = 20001},
        data_1 = {ip = "::1", port = 20002},
    },
}
