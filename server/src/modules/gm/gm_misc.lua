-- gm_misc.lua
-- 2018-04-11
-- xzc

-- 一些全局的gm，没有具体模块的gm可以放在这里，其他的需要在对应的模块里注册

-- 热更
GM.reg("reload", function()
    print("reload ...")
end, -1)

-- ping一下服务器间的延迟，看卡不卡
GM.reg("ping", function()
    Ping.start(1)
end, W_GAME | W_MAIN)

return GM
