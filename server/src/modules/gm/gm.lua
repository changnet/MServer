-- gm.lua
-- 2018-04-11
-- xzc

-- gm处理
GM = {}

local gm_data = {}
local forward_map = {}

-- 检测聊天信息是否为gm，如果是就执行
function GM.chat_gm(player, context)
    if not g_setting.gm then return false end

    -- string.byte("@") == 64
    if string.byte(context, 1) ~= 64 then return false end

    local ok, msg = GM.exec("chat", player, context)
    if not ok then
        print("GM exec error", msg)
        -- 仍然返回true表示已执行gm
    end

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
        eprint("gm auto forward no conn found:%s", cmd)
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
    local gm_func = gm_data[cmd] or GM[cmd]
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

-- 注册gm指令
-- @param wtype 执行该gm的worker类型，默认为player
-- @param level gm权限等级，默认为10，数值越大权限越高
function GM.reg(cmd, func, wtype, role)
    gm_data[cmd] = {
        func = func,
        wtype = wtype or W_PLAYER,
        role = role or 10,
    }
end

return GM
