-- define.lua 服务器全局定义
-- 2017-03-28
-- xzc

-- >>>>>>>>>>>>>>> !!!此文件定义的内容与业务无关，不能热更!!! <<<<<<<<<<<<<<<<<<<< 

-- 服务器索引定义
SRV_NAME =
{
    gateway = 1,
    world   = 2,
    test    = 3,
    example = 4
}

return { ["SRV_NAME"] = SRV_NAME }