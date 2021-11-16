-- 负责所有状态统计管理
local Statistic_mgr = oo.singleton(...)

-- 根据名字获取统计对象
-- @param name 唯一统计名字，如rpc、cmd
function Statistic_mgr:get(name)
    return nil
end

--[[

-- 更新耗时统计
function CommandMgr:update_statistic(stat_list, cmd, ms)
    local stat = stat_list[cmd]
    if not stat then
        stat = {ms = 0, ts = 0, max = 0, min = 0}
        stat_list[cmd] = stat
    end

    stat.ms = stat.ms + ms
    stat.ts = stat.ts + 1
    if ms > stat.max then stat.max = ms end
    if 0 == stat.min or ms < stat.min then stat.min = ms end
end

-- 写入耗时统计到文件
function CommandMgr:raw_serialize_statistic(path, stat_name, stat_list)
    local stat_cmd = {}
    for k in pairs(stat_list) do table.insert(stat_cmd, k) end

    -- 按名字排序，方便对比查找
    table.sort(stat_cmd)

    Log.raw_file_printf(path, "%s", stat_name)

    for _, cmd in pairs(stat_cmd) do
        local stat = stat_list[cmd]
        Log.raw_file_printf(path, "%-16d %-16d %-16d %-16d %-16d %-16d",
                                  cmd, stat.ts, stat.ms, stat.max, stat.min,
                                  math.ceil(stat.ms / stat.ts))
    end
end

-- 写入耗时统计到文件
function CommandMgr:serialize_statistic(reset)
    if not self.cmd_perf then return false end

    local path = string.format("%s_%s", self.cmd_perf, g_app.name)

    Log.raw_file_printf(path, "%s ~ %s:", time.date(self.stat_tm),
                              time.date(ev:time()))
    -- 指令 调用次数 总耗时(毫秒) 最大耗时 最小耗时 平均耗时
    Log.raw_file_printf(path, "%-16s %-16s %-16s %-16s %-16s %-16s",
                              "cmd", "count", "msec", "max", "min", "avg")

    self:raw_serialize_statistic(path, "cs_cmd:", self.cs_stat)
    self:raw_serialize_statistic(path, "ss_cmd:", self.ss_stat)
    self:raw_serialize_statistic(path, "css_cmd:", self.css_stat)

    Log.raw_file_printf(path, "%s.%d end %s", g_app.name, g_app.index,
                              "\n\n")
    if reset then
        self.cs_stat = {}
        self.ss_stat = {}
        self.css_stat = {}
        self.stat_tm = ev:time()
    end

    return true
end
]]

--[[

-- 更新耗时统计
function Rpc:update_statistic(method_name, ms)
    local stat = self.stat[method_name]
    if not stat then
        stat = {ms = 0, ts = 0, max = 0, min = 0}
        self.stat[method_name] = stat
    end

    stat.ms = stat.ms + ms
    stat.ts = stat.ts + 1
    if ms > stat.max then stat.max = ms end
    if 0 == stat.min or ms < stat.min then stat.min = ms end
end

-- 写入耗时统计到文件
function Rpc:serialize_statistic(reset)
    if not self.rpc_perf then return false end

    local path = string.format("%s_%s", self.rpc_perf, g_app.name)

    local stat_name = {}
    for k in pairs(self.stat) do table.insert(stat_name, k) end

    -- 按名字排序，方便对比查找
    table.sort(stat_name)

    Log.raw_file_printf(path, "%s ~ %s:", time.date(self.stat_tm),
                              time.date(ev:time()))

    -- 方法名 调用次数 总耗时(毫秒) 最大耗时 最小耗时 平均耗时
    Log.raw_file_printf(path, "%-32s %-16s %-16s %-16s %-16s %-16s",
                              "method", "count", "msec", "max", "min", "avg")

    for _, name in pairs(stat_name) do
        local stat = self.stat[name]
        Log.raw_file_printf(path, "%-32s %-16d %-16d %-16d %-16d %-16d",
                                  name, stat.ts, stat.ms, stat.max, stat.min,
                                  math.ceil(stat.ms / stat.ts))
    end

    Log.raw_file_printf(path, "%s.%d end %s", g_app.name, g_app.index,
                              "\n\n")

    if reset then
        self.stat = {}
        self.stat_tm = ev:time()
    end

    return true
end
]]

local mgr = Statistic_mgr()

return mgr
