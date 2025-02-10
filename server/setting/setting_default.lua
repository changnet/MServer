-- 服务器 配置 样本
-- 任何配置都要写到配置样本，并上传版本管理(svn、git...)
-- 各个服部署时配置从样本修改而来，但不要上传。避免配置被覆盖

return
{
    mongo_ip = "127.0.0.1", -- mongodb ip
    mongo_port = "27013", -- mongodb 端口
    mongo_db = "test_999", -- 需要连接的数据库
    mongo_user = "test", -- mongo 用户(以后弄个加密，以免明文保存)
    mongo_pwd  = "test", -- mongo 密码(以后弄个加密，以免明文保存)

    mysql_ip   = "127.0.0.1";
    mysql_port = 3306,
    mysql_user = "test",
    mysql_pwd  = "test",
    mysql_db   = "test_999",

    gm = true, -- 是否启用gm
    lang = "zh", -- 简体中文
    server = 1, -- 当前服务器id
    process = "gateway", -- 当前进程启动worker名字

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
        -- 同一个类型的节点，只能有一个master(可以没有master)
        -- 一个节点的port = 0表示该节点只接收master中转的数据，不开启监听
        gateway1 = {host = "::1", port = 20001, master = 1},
        gateway2 = {host = "::1", port = 20002},
        db1 = {host = "::1", port = 20003},
        db2 = {host = "::1", port = 20004},
    },
    -- 当前进程启动集群节点范围
    -- 假如当前为gateway，则{1, 10}表示表示 gateway1~gateway10
    cluster_range = {1, 10},
}
