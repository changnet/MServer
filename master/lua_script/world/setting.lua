-- world 配置
-- 任何配置都要写到配置样本，并上传版本管理(svn、git...)
-- 各个服的配置从样本修改而来，但不要上传。避免配置被覆盖

return
{
    sip     = "127.0.0.1", -- s2s监听ip
    sport   = 20001,       -- s2s监听端口
    servers =
    {
        { ip = "127.0.0.1",port = 10001 }, -- gateway
    },

    mongo_ip = "127.0.0.1", -- mongodb ip
    mongo_port = "27013", -- mongodb 端口
    mongo_db = "test_999", -- 需要连接的数据库
    mongo_user = "test", -- mongo 用户(以后弄个加密，以免明文保存)
    mongo_pwd  = "test", -- mongo 密码(以后弄个加密，以免明文保存)
}
