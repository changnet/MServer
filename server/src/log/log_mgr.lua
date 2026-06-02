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

local time_cache
local time_cache_str
local time_func = os.time

local function flush_table_logs(mysql, table_name, rows)
    local e = mysql:align_insert(table_name, rows)
    if e ~= 0 then
        local e1, str = mysql:error()
        eprint("log mysql insert error", e,  e1, str)
    end
    this.buffer[table_name] = {}
end

local function flush_logs()
    local mysql = this.mysql

    -- 还在startup中
    if not mysql.connected then
        eprint("log mgr flush db not connect")
        return 0
    end

    if 0 ~= mysql:ping() then
        local e, str = mysql:error()
        eprintf("log mysql ping error %d %s", e, str or "")
        return 0
    end

    local count = 0
    for table_name, rows in pairs(this.buffer) do
        local n = #rows
        if n > 0 then
            count = count + n
            flush_table_logs(mysql, table_name, rows)
        end
    end

    return count
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
    this.mysql:connect_later(conf.ip, conf.port, conf.user, conf.pwd, conf.db, nil,
    function(mysql)
        mysql:read_database_struct()
        printf("log mgr mysql connected")
    end)

    return true
end

local function stop()
    local mysql = this.mysql

    -- 还在startup中
    if not mysql or not mysql.connected then
        eprint("log mgr db not connect")
        return
    end

    local count = flush_logs()

    this.mysql:disconnect()
    this.mysql = nil

    printf("log mgr stopped, flush %d logs", count or 0)
end

local function fmt_time()
    local now = time_func()
    if now ~= time_cache then
        time_cache = now
        time_cache_str = os.date("%Y-%m-%d %H:%M:%S", now)
    end

    return time_cache_str
end

function LogMgr.db(name, tbl)
    if not tbl.time then tbl.time = fmt_time() end

    add_log(name, tbl)
end

function LogMgr.misc(pid, op, v1, v2, v3)
    add_log("log_misc", {
        pid = pid or 0,
        op = op or 0,
        val = tostring(v1 or ""),
        val1 = tostring(v2 or ""),
        val2 = tostring(v3 or ""),
        time = fmt_time(),
    })
end

function LogMgr.pmisc(pid, op, v1, v2, v3)
    add_log("log_misc", {
        pid = pid or 0,
        op = op or 0,
        val = tostring(v1 or ""),
        val1 = tostring(v2 or ""),
        val2 = tostring(v3 or ""),
        time = fmt_time(),
    })
end

Shutdown.reg({
    name = "log_mgr",
    func = stop,
})

Event.reg(EV.MIN_TIMER, flush_logs)

return LogMgr
