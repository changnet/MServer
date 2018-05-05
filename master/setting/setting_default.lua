-- 服务器 配置 样本
-- 任何配置都要写到配置样本，并上传版本管理(svn、git...)
-- 各个服部署时配置从样本修改而来，但不要上传。避免配置被覆盖

return
{
    gateway = -- gateway 配置
    {
        sip   = "127.0.0.1", -- s2s监听ip
        sport = 10001,       -- s2s监听端口
        cip   = "0.0.0.0", -- c2s监听ip，在virtualbox的端口转发模式下，127.0.0.1转发不成功
        cport = 10002,       -- s2s监听端口
        hip   = "127.0.0.1", -- http监听ip
        hport = 10003,       -- http监听端口
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
    },

    world = -- world服务器配置
    {
        sip     = "127.0.0.1", -- s2s监听ip
        sport   = 20001,       -- s2s监听端口
        servers =
        {
            { ip = "127.0.0.1",port = 10001 }, -- gateway
        },

        gm = true, -- 是否启用gm
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
    }
}
