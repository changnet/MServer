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
    if not mysql then
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
local function start()
    this.mysql = MySQL()
    this.mysql:thread_init()
    this.mysql:set_ssl(false)

    local conf = g_setting.mysql
    local e = this.mysql:connect(conf.ip, conf.port, conf.user, conf.pwd, conf.db)
    if e ~= 0 then
        eprint("log mysql connect error", this.mysql:error())
    end

    Timer.interval(5000, 5000, -1, flush_logs)
end

local function stop()
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

SE.reg(SE_WORKER_ME_READY, start)
SE.reg(SE_WORKER_STOP, stop)

return LogMgr
