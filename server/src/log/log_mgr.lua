-- log_mgr.lua
-- 2018-04-24
-- xzc

-- 日志管理，负责日志入库和上报到后台、渠道等
LogMgr = {}

local MySQL = require "mysql.mysql"

local MAX_BATCH_SIZE = 256
local this = memory("LogMgr", {
    buffer = {} -- this.buffer[table_name] = {rows...}
})

local function flush_table_logs(mysql, table_name, rows)
    local e = mysql:insert(table_name, rows)
    if e ~= 0 then
        eprintf("log mysql insert error %d %s", e, mysql:error() or "")
    end
    this.buffer[table_name] = {}
end

local function flush_logs()
    local mysql = this.mysql

    -- 还在startup中
    if not mysql.connected then return end

    if 0 ~= mysql:ping() then
        local e, str = mysql:error()
        eprintf("log mysql ping error %d %s", e, str or "")
        return
    end

    for table_name, rows in pairs(this.buffer) do
        if #rows > 0 then
            flush_table_logs(mysql, table_name, rows)
        end
    end
end

local function add_log(table_name, row)
    local rows = this.buffer[table_name]
    if not rows then
        rows = {}
        this.buffer[table_name] = rows
    end
    table.insert(rows, row)
    if #rows >= MAX_BATCH_SIZE then
        flush_table_logs(this.mysql, table_name, rows)
    end
end

-- 模块启动回调
function LogMgr.start()
    this.mysql = MySQL()

    local conf = g_setting.mysql
    this.mysql:connect_later(conf.ip, conf.port, conf.user, conf.pwd, conf.db)

    Timer.interval(5000, 5000, -1, flush_logs)

    return true
end

local function stop()
    local mysql = this.mysql

    -- 还在startup中
    if not mysql or not mysql.connected then return end

    flush_logs()

    this.mysql:disconnect()
    this.mysql = nil
end

function LogMgr.db(name, tbl)
    add_log(name, tbl)
end

function LogMgr.misc(pid, op, v1, v2, v3)
    add_log("misc", {
        pid = pid or 0,
        op = op or 0,
        val = tostring(v1 or ""),
        val1 = tostring(v2 or ""),
        val2 = tostring(v3 or ""),
        time = os.date("%Y-%m-%d %H:%M:%S")
    })
end

function LogMgr.pmisc(pid, op, v1, v2, v3)
    add_log("misc", {
        pid = pid or 0,
        op = op or 0,
        val = tostring(v1 or ""),
        val1 = tostring(v2 or ""),
        val2 = tostring(v3 or ""),
        time = os.date("%Y-%m-%d %H:%M:%S")
    })
end

Shutdown.reg({
    name = "log_mgr",
    func = stop,
})
Rtti.name_func("LogMgr.flush_logs", flush_logs)

return LogMgr
