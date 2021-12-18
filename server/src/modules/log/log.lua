-- log_mgr.lua
-- 2018-04-24
-- xzc
-- 日志管理

--[[
关于插入效率

1. bulk insert，合并批量插入，注意语句不要超长了
insert into log value (1,2), (1,2)

SHOW VARIABLES LIKE 'max_allowed_packet';
max_allowed_packet=1048576 or 1 MiB

2. 使用事务，这种和bulk insert效率差不多，但需要3条指令。如果
start transaction;
insert into log value (1, 2)
insert into log value (1, 2)
insert into log value (1, 2)
insert into log value (1, 2)
commit;

3. load data直接从文件加载，这个应该是最快的
https://dev.mysql.com/doc/refman/8.0/en/load-data.html

load data infile "log.txt"

但是要考虑下转义的问题
https://dev.mysql.com/doc/refman/8.0/en/problems-with-null.html
When reading data with LOAD DATA, empty or missing columns are updated with ''. To load a NULL value into a column, use \N in the data file. The literal word NULL may also be used under some circumstances

从内存load data而不是从文件
https://mariadb.com/ja/resources/blog/customized-mysql-load-data-local-infile-handlers-with-libmysqlclient/

LOAD DATA INFILE is unsafe for statement-based replication
注意, load data是需要额外的权限的，需要运维给权限

日志主要是量多，插入频繁。对于这个场景INNODB这个引擎可能不是最优的，但效率也还可以接受。
后台分析日志时，需要用到一些特性，因此选择这个引擎。曾见过渠道强制指定使用INNODB的
]]

--[[
关于日志管理

日志的量很大，很容易就把一张表写得很大，然后出现效率问题。但日志有很强的时效性，超过一定
时间的数据都是冷数据，甚至可以丢弃（比如6个月之前的日志完全可以丢掉），因此需要利用这一
点做针对性优化，通常的办法是分表或者分区

分表
分表就是按日期建表，例如 log_202101, log_202102, log_202103, ...
日志按对应的日期插入到对应的表中。当然也可以只插入log表，运维定时把旧日志转移到特定日期
表中。
这种方式就是维护麻烦，需要定期建表，查询时也需要按日期查表

分区
CREATE TABLE ti (id INT, d DATETIME)
    ENGINE=INNODB
    PARTITION BY HASH( MONTH(d) )
    PARTITIONS 12;
分区就是只建一张表，利用MySQL的分区功能，按日期自动把日志分到不同的分区中。然后运维定期
去删除可丢弃的数据即可。
这种方式查询不同月份时不需要跨表，也不需要定期建表，但实际上也有很多问题。
1. PARTITION BY的条件不能使用运算符，按月分区必须使用DATETIME，不能使用时间戳
2. Partition Pruning不生效https://dev.mysql.com/doc/refman/8.0/en/partitioning-pruning.html
When a table is partitioned by HASH or [LINEAR] KEY, pruning can be used only on
integer columns. For example, this statement cannot use pruning because dob is a
DATE column:
SELECT * FROM t4 WHERE dob >= '2001-04-14' AND dob <= '2005-10-15';
对于日志而言，按时间筛选是很常用的，而这里偏偏没法用。为了解决这个问题，需要显式使用
Partition Selection(>= MySQL 5.6)
SELECT * FROM t4 PARTITION (p1,p2) WHERE dob >= '2001-04-14' AND dob <= '2005-10-15';
语句有没有命中Partition Pruning，可以使用explain partition ...来查看
3. 如果使用了分区，则所有的primary key或者unique index都需要包含分区所用的字段
    这对使用了自增id，基于使用了外键的表简直是灾难
    那日志中干脆就不使用自增id，也不用primary key了，如果用，就把日期加上去做联合主键

为了解决上面的分区问题，还有一些特殊处理
1. 使用range分区，但这个需要定时维护分区的
PARTITION BY RANGE (extract(year_month from `d`))
(PARTITION P0 VALUES LESS THAN (202012) ENGINE = InnoDB,
 PARTITION P1 VALUES LESS THAN (202104) ENGINE = InnoDB,
 PARTITION P2 VALUES LESS THAN (202108) ENGINE = InnoDB,
 PARTITION P3 VALUES LESS THAN (202112) ENGINE = InnoDB,
 PARTITION P4 VALUES LESS THAN MAXVALUE ENGINE = InnoDB)

2. 使用BIGINT的类型而不是DATETIME,2001-04-14 直接存成数字 20010414，这样就可以
    PARTITION BY HASH( d % 100 )
    PARTITIONS 12;
    但这样秒数需要分开存，查询日志时间需要分开不同字段

对比下来还是使用显式使用Partition Selection来得方便
]]

require "modules.log.log_header"
local Mysql = require "mysql.mysql"

-- 日志模块
Log = {}

-- 单条sql长度，max_allowed_packet
-- 需要预留最后一条sql语句超出的长度以及insert部分长度
local MAX_STAT_LEN = 1048576 - 48576

local SCHEDULE = {
    misc = {
        sql = "INSERT INTO misc (pid,op,val,val1,val2,vals,time) VALUES "
    },
    res = {
        sql = "INSERT INTO res (pid,op,id,change,val1,val2,vals,time) VALUES "
    },
    stat = {
        sql = "INSERT INTO stat (pid,stat,val,time) VALUES ",
        update = "ON DUPLICATE KEY UPDATE val=VALUES(val),time=VALUES(time)"
    }
}

local this = global_storage("Log", {
    values = {}
})

-- 异步文件写入线程
local async_file = g_async_log
--//////////////////////////////////////////////////////////////////////////////

-- 写入到日志文件
function Log.file(path, text)
end

-- 自定义写文件
function Log.raw_file_printf(path, ...)
    return async_file:append_log_file(path, string.format(...))
end

--//////////////////////////////////////////////////////////////////////////////

local t_i
local t_s

-- 使用日志产生时的时间，而不是插入数据库的时间，因为有缓存
local function db_time()
    local t = ev:time()

    -- 这个时间缓存，这样同一秒插入时，就不需要频繁格式化字符串
    if t ~= t_i then
        t_s = "\"" .. time.date(t) .. "\""
        t_i = t
    end

    return t_s
end

-- 转换为db字段的字符串
local function to_db_str(val)
    if nil == val then return "null" end

    -- 拼接sql时，字符串要加双引号
    if "string" == type(val) then
        -- https://dev.mysql.com/doc/c-api/8.0/en/mysql-real-escape-string.html
        -- Characters encoded are \, ', ", NUL (ASCII 0), \n, \r, and Control+Z.
        -- Strictly speaking, MySQL requires only that backslash and the quote
        -- character used to quote the string in the query be escaped.
        -- mysql_real_escape_string() quotes the other characters to make them
        -- easier to read in log files
        -- 这里做个简单的转义，防止字符串里包含特殊字符时报错
        return "\"" .. val:gsub("[\\'\"]") .. "\""
    end

    return tostring(val)
end

-- 把多个字段折叠起来存到vals字段
local function to_db_vals(val, ...)
    if nil == val then return "null" end

    -- 使用#号而不是;因为分号是cvs文件用的，从数据库导出cvs的时候就乱了
    return table.concat({val, ...}, "#")
end

-- 执行db缓存的日志
local function exec_db_values(tbl_name, values, i, j)
    local schedule = SCHEDULE[tbl_name]
    local stat = nil
    if schedule.update then
        stat = schedule.sql
            .. table.concat(values, ",", i, j) .. schedule.update
    else
        stat = schedule.sql .. table.concat(values, ",", i, j)
    end

    -- TODO 底层的sql缓存是给小语句做优化的
    -- 这种超大的sql是实时分配内存，没有缓存，后面看看是否需要优化
    this.db:exec_cmd(stat)
end

-- 执行单个db日志表的日志缓存
local function exec_one_db_table(tbl_name)
    local values = this.values[tbl_name]
    if not values then return end

    local max_log = #values
    if max_log <= 0 then return end

    this.values[tbl_name] = nil
    print("exec db log", tbl_name, max_log)

    local i = 1
    local max_len = 0
    for index, str in ipairs(values) do
        max_len = max_len + string.len(str)
        if max_len >= MAX_STAT_LEN then
            exec_db_values(tbl_name, values, i, index)
            i = index + 1
        end
    end

    if i <= max_log then
        exec_db_values(tbl_name, values, i, max_log)
    end
end

-- 执行所有db日志
local function exec_db()
    for tbl_name in pairs(this.values) do
        exec_one_db_table(tbl_name)
    end
end

-- 添加db日志（仅缓存）
local function append_db(tbl_name, fmt, ...)
    local str = string.format(fmt, ...)
    local values = this.values[tbl_name]
    if not values then
        values = {}
        this.values[tbl_name] = values
    end
    table.insert(values, str)

    -- 如果已经累计太多了，立即执行，否则等定时器定时刷入
    if #values > 4096 then
        print("too many db log, exec now", tbl_name)
        exec_one_db_table(tbl_name)
    end
end

-- 记录资源日志
function Log.db_res(player, id, change, count, op, val)
    -- `pid` bigint(64) NOT NULL DEFAULT '0' COMMENT '玩家pid，0表示系统日志',
    -- `op` int(11) DEFAULT '0' COMMENT '操作日志id(详见后端日志定义)',
    -- `id` int(11) DEFAULT '0' COMMENT '资源id',
    -- `change` int(11) NOT NULL COMMENT '资源变化数量',
    -- `val1` varchar(512) DEFAULT NULL COMMENT '额外数据1',
    -- `val2` varchar(512) DEFAULT NULL COMMENT '额外数据2',
    -- `vals` varchar(512) DEFAULT NULL COMMENT '额外数据,剩下的额外数据都自动拼接到这里',
    -- `time` DATETIME NOT NULL COMMENT '操作时间',
    return append_db("res", "(%d,%d,%d,%d,%s,%s,%s,%s)",
        player.pid, op, id, change, to_db_str(val), "null", "null", db_time())
end

-- 记录状态status到数据库，只记录一个，如果存在则覆盖
function Log.db_stat(pid, stat, val)
    -- `pid` bigint(64) NOT NULL DEFAULT '0' COMMENT '玩家pid，0表示系统日志',
    -- `stat` int(11) DEFAULT '0' COMMENT '状态定义(详见后端定义)',
    -- `val` varchar(2048) DEFAULT NULL COMMENT '额外数据1',
    -- `time` DATETIME NOT NULL COMMENT '操作时间',
    return append_db("stat", "(%d,%d,%s,%s)",
        pid, stat, to_db_str(val), db_time())
end

-- 记录日志到数据库(无玩家对象)
function Log.db_pid_misc(pid, op, val, val1, val2, ...)
    -- `pid` bigint(64) NOT NULL DEFAULT '0' COMMENT '玩家pid，0表示系统日志',
    -- `op` int(11) DEFAULT '0' COMMENT '操作日志id(详见后端日志定义)',
    -- `val` varchar(512) DEFAULT NULL COMMENT '操作的变量',
    -- `val1` varchar(512) DEFAULT NULL COMMENT '额外数据1',
    -- `val2` varchar(512) DEFAULT NULL COMMENT '额外数据2',
    -- `vals` varchar(512) DEFAULT NULL COMMENT '额外数据,剩下的额外数据都自动拼接到这里',
    -- `time` DATETIME NOT NULL COMMENT '操作时间',
    return append_db("misc", "(%d,%d,%s,%s,%s,%s,%s)",
        pid, op, to_db_str(val), to_db_str(val1),
        to_db_str(val2), to_db_vals(...), db_time())
end

-- 记录一些杂项日志到数据库
function Log.db_misc(player, op, val, val1, val2, ...)
    return Log.db_pid_misc(player.pid, op, val, val1, val2, ...)
end

-- /////////////////////////////////////////////////////////////////////////////
-- 初始化db日志
local function on_app_start(check)
    if check then
        return this.ok
    end
    this.db = Mysql()
    this.db:start(g_setting.mysql_ip, g_setting.mysql_port,
                         g_setting.mysql_user, g_setting.mysql_pwd,
                         g_setting.mysql_db, function()
            this.ok = true
    end)
end

-- 关闭文件日志线程及数据库日志线程
local function on_app_stop()
    -- self.async_file:stop()
    if this.db then
        exec_db()
        this.db:stop()
    end

    return true
end
App.reg_start("Log", on_app_start)
App.reg_stop("Log", on_app_stop, 28) -- 关闭优先级低，需要等其他模块写完日志

return Log
