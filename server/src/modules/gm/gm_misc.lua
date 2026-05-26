-- gm_misc.lua
-- 2018-04-11
-- xzc

-- 一些全局的gm，没有具体模块的gm可以放在这里，其他的需要在对应的模块里注册

-- 热更
GM.reg("reload", function()
    Misc.reload()
    return true
    -- -1在gm那边会广播到所有线程执行
end, W.MAIN)

-- 启用调试日志打印
GM.reg("debug", function(pid, p1)
    if "1" == p1 then
        Log.enable_debug(true)
        print("enable debug log")
    else
        Log.enable_debug(false)
        print("disable debug log")
    end
    return true
end, -1)

-- ping一下服务器间的延迟，看卡不卡
GM.reg("ping", function()
    Ping.start(1)
end, W.GAME | W.MAIN)

-- 执行测试命令
GM.reg("test", function(player, cmd, ...)
    local IntegrateTest = require "test.integrate_test"
    IntegrateTest.run(player, cmd, ...)
    return true
end)

-- 添加任意资源
GM.reg("res", function(player, id, num)
    id = tonumber(id)
    num = tonumber(num)
    if not id or not num or 0 == num then
        eprint("gm invalid param", id, num)
        return
    end

    if num > 0 then
        Res.add(player, {{id = id, num = num}}, LOG.GM)
    else
        Res.dec(player, {{id = id, num = num}}, LOG.GM)
    end

    return true
end)

return GM
