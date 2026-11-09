-- misc.lua
-- 2018-02-06
-- xzc

-- 一些杂七杂八的小接口
Misc = {}

-- 空函数，有些rpc需要调用一个空函数来保证数据完整性
function Misc.dummy()
end

-- 热更重入保护超时（秒）。热更异常中断时，超时后才允许强制热更
local RELOAD_TIMEOUT = 300

-- 重载脚本
function Misc.reload()
    -- worker由主节点广播触发，没有完成回调，因此重入判断只在主节点做
    local is_main = LOCAL_ADDR == MAIN_ADDR
    local now = os.time()
    if is_main and _G.__reloading then
        local elapsed = now - _G.__reloading
        if elapsed < RELOAD_TIMEOUT then
            printf("script is reloading, retry after %d seconds",
                RELOAD_TIMEOUT - elapsed)
            return false
        end
        -- 上次热更异常中断且已超时，允许强制重更
        printf("script reload timeout, force reload")
    end
    _G.__reloading = now

    local t1 = Engine.steady_clock()
    local m1 = collectgarbage("count")

    __unrequire()
    Startup.load(true)

    local m2 = collectgarbage("count")
    local t = Engine.steady_clock() - t1

    local info = {
            t = t, m1 = m1, m2 = m2, name = LOCAL_NAME,
    }
    if not is_main then
        _G.__reloading = nil -- worker本地热更已完成
        return info
    end

    -- 先收集addr再发消息，避免回调在遍历过程中修改wait_size
    local addrs = {}
    for addr in pairs(WorkerData) do
        addrs[#addrs + 1] = addr
    end

    local wait_size = #addrs
    local info_tbl = {info}
    local reload_finish = function(ret_info)
        wait_size = wait_size - 1
        if ret_info then table.insert(info_tbl, ret_info) end
        if wait_size > 0 then return end

        -- 所有节点都热更完成，解除主节点的重入保护
        _G.__reloading = nil

        table.sort(info_tbl, function(a, b) return a.name < b.name end)

        local str_tbl = {}
        for _, d in pairs(info_tbl) do
            local str = string.format(
                "    name = %s, time = %d, m1 = %.0f, m2 = %.0f",
                d.name, d.t, d.m1, d.m2
            )
            table.insert(str_tbl, str)
        end

        local t2 = Engine.steady_clock() - t1
        printf("script reloaded, time %d ms\n%s",
            t2, table.concat(str_tbl, "\n"))
    end
    for _, addr in ipairs(addrs) do
        Callback[addr].Misc.reload().done(reload_finish)
    end

    -- 没有worker时回调不会触发，本地热更完成即收尾
    if 0 == #addrs then reload_finish() end
end

-- 执行一个文件中的函数(一般是调试用，不能用于业务逻辑)
function Misc.eval_file(file, func_name, ...)
    local path, err = package.searchpath(file, package.path)
    if not path then error(err) end

    local f, msg = loadfile(path)
    if not f then error(msg) end

    local m = f()
    local func = m[func_name]
    if not func then
        error(string.format("no func %s found in %s", func_name, file))
    end

    return func(...)
end

return Misc
