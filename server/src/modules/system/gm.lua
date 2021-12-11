-- gm.lua
-- 2018-04-11
-- xzc

-- gm处理
GM = {}

local gm_map = {}
local forward_map = {}

-- 检测聊天中是否带gm
function GM.chat_gm(player, context)
    if not g_setting.gm then return false end

    GM.exec("chat", player, context)

    return true
end

-- 转发或者运行gm
function GM.exec(where, player, context)
    if not string.start_with(context, "@") then
        return false, "gm need to start with@:" .. context
    end

    local raw_ctx = string.sub(context, 2) -- 去掉@

    -- 分解gm指令(@level 999分解为{@level,999})
    local args = string.split(raw_ctx, " ")

    -- 自动转发到其他进程
    if GM.auto_forward(where, player, args[1], args) then return true end

    return GM.raw_exec(where, player, table.unpack(args))
end

-- 自动转发gm到对应的服务器
-- TODO:暂时不考虑存在多个同名服务器的情况，有同名的需要对应的gm逻辑里自己转
function GM.auto_forward(where, player, cmd, args)
    local app_type = forward_map[cmd]
    if not app_type or APP_TYPE == app_type then return false end

    local session = g_app:encode_session(app_type, 1, g_app.id)
    local srv_conn = SrvMgr.get_conn_by_session(session)
    if not srv_conn then
        elog("gm auto forward no conn found:%s", cmd)
        return true
    end

    Rpc.proxy(srv_conn):rpc_gm(g_app.name, cmd, table.unpack(args))
    return true
end

-- gm指令运行入口（注意Player对象可能为nil，因为有可能从http接口调用gm）
function GM.raw_exec(where, player, cmd, ...)
    -- 防止日志函数本身报错，连热更的指令都没法刷了
    xpcall(print, __G__TRACKBACK, "exec gm:", where, cmd, ...)

    -- 优先查找注册过来的gm指令，然后是gm模块本身的指令
    local gm_func = gm_map[cmd] or GM[cmd]
    if gm_func then
        local ok, msg = gm_func(player, ...)
        if ok == nil then
            return true -- 很多不重要的指令默认不写返回值，认为是成功的
        else
            return ok, msg
        end
    end

    print("no such gm", cmd)
    return false, "no such gm:" .. tostring(cmd)
end

-- 广播gm
function GM.broadcast(cmd, ...)
    -- 仅允许网关广播
    assert(cmd and APP_TYPE == GATEWAY)

    -- 可能有多个不现类型的进程连接到服务器（如中心服、连服）等
    -- 只广播给当前服务器的进程（如场景服）
    local DST = {
        [GATEWAY] = true,
        [WORLD] = true,
        [AREA] = true,
    }
    local conn_list = SrvMgr.get_all_srv_conn()
    for _, srv_conn in pairs(conn_list) do
        if srv_conn.auth then
            local app_type = srv_conn.app_type
            if DST[app_type] then
                Rpc.conn_call(srv_conn, GM.raw_exec, g_app.name, nil, cmd, ...)
            end
        end
    end
end

-- 注册gm指令
-- @param app_type 真正运行该gm的服务器类型
function GM.reg(cmd, gm_func, app_type)
    gm_map[cmd] = gm_func
    forward_map[cmd] = app_type
end

--------------------------------------------------------------------------------

-- 全局热更，会更新其他进程
function GM.hf()
    hot_fix()
    if GATEWAY == APP_TYPE then GM.broadcast("hf") end
end

-- ping一下服务器间的延迟，看卡不卡
function GM.ping()
    return Ping.start(1)
end

-- 添加元宝
function GM.add_gold(player, count)
    player:add_gold(tonumber(count), LOG.GM)
end

-- 添加道具
function GM.add_item(player, id, count)
    local bag = player:get_module("bag")
    bag:add(tonumber(id), tonumber(count), LOG.GM)
end

-- 邮件测试
function GM.sys_mail(player, title, ctx)
    g_mail_mgr:send_sys_mail(title, ctx)
end

-- 发送邮件测试
function GM.send_mail(player, pid, title, ctx)
    g_mail_mgr:send_mail(tonumber(pid), title, ctx)
end

return GM
