-- hot_fix.lua
-- 2018-04-12
-- xzc
-- 热更逻辑

-- 全局热更
function hot_fix()
    local ms = ev:steady_clock()

    -- 清除脚本
    __unrequire()

    -- 重新加载入口文件
    App.load_module()

    printf("hot fix finish,time elapsed %d ms", ev:steady_clock() - ms)
end
