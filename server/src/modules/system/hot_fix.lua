-- hot_fix.lua
-- 2018-04-12
-- xzc
-- 热更逻辑


-- 热更脚本
local function fix_script()
    -- 清除脚本
    unrequire()
    -- 重新加载入口文件
    g_app:module_initialize()
end

-- 全局热更
function hot_fix()
    local ms = ev:real_ms_time()

    fix_script()

    printf("hot fix finish,time elapsed %d ms", ev:real_ms_time() - ms)
end

-- 只热更脚本，调试脚本时更快
function hot_fix_script()
    local ms = ev:real_ms_time()

    fix_script()

    printf("hot fix script finish,time elapsed %d ms", ev:real_ms_time() - ms)
end
