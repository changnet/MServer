-- gm_misc.lua
-- 2018-04-11
-- xzc

-- 一些全局的gm，没有具体模块的gm可以放在这里，其他的需要在对应的模块里注册

-- 热更
GM.reg("reload", function()
    __unrequire()
    Startup.load(true)
end, -1)

-- 启用调试日志打印
GM.reg("debug", function(pid, p1)
    if "1" == p1 then
        Log.enable_debug(true)
        print("enable debug log")
    else
        Log.enable_debug(false)
        print("disable debug log")
    end
end, -1)

-- ping一下服务器间的延迟，看卡不卡
GM.reg("ping", function()
    Ping.start(1)
end, W.GAME | W.MAIN)

-- 执行测试命令
GM.reg("test", function(player, cmd, ...)
    local T = require "test.integrate_test"
    T.run(player, cmd, ...)
end)

-- 添加任意资源
GM.reg("res", function(player, id, num)
    id = tonumber(id)
    num = tonumber(num)
    if not id or not num then
        eprint("gm invalid param", id, num)
        return
    end

    Res.add(player, {{id = id, num = num}}, LOG.GM)
end)

return GM
