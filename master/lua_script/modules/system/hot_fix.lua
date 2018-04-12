-- hot_fix.lua
-- 2018-04-12
-- xzc

-- 热更逻辑

-- 热更协议
local function fix_proto()
    if g_command_mgr.modify or g_rpc.modify then
        local _pkt = g_command_mgr:command_pkt()
        g_network_mgr:srv_multicast( SS.SYS_CMD_SYNC,_pkt )
    end
end

-- 热更schema文件
local function fix_schema()
    g_command_mgr:load_schema()
end

-- 热更脚本
local function fix_script()
    -- 清除脚本
    unrequire()
    -- 重新加载入口文件
    g_app:module_initialize()
end

-- 全局热更
function hot_fix()
    local sec, usec = util.timeofday()

    fix_script()
    fix_proto()
    fix_schema()

    local nsec, nusec = util.timeofday()
    local msec = (nsec - sec)*1000000 + nusec - usec
    PLOG( "hot fix finish,time elapsed %d microsecond",msec ) -- 微秒
end

-- 只热更脚本，调试脚本时更快
function hot_fix_sc()
    local sec, usec = util.timeofday()

    fix_script()

    local nsec, nusec = util.timeofday()
    local msec = (nsec - sec)*1000000 + nusec - usec
    PLOG( "hot fix script finish,time elapsed %d microsecond",msec ) -- 微秒
end
