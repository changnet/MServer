-- mysql.lua
-- 2015-11-20
-- xzc

--[[
mysql数据存储处理

1. decimal在lua中是用double来表示，精度可能会不够
2. MySQL中支持boolean类型，但实际是以tinyint类型保存，无法区分原始类型。因此lua中不支持boolean类型，只支持数字
3. binary(10)这种定长的数据，在mysql中返回长度永远是固定的，无法得知存入数据的长度。因此即使只存入abc，返回的也是abc0000000
    由于包含二进制数据，也是不能用strlen之类的函数判断
]]

local MySql = require "engine.MySql"

local MySQL = oo.class()

local DEBUG = true -- 是否启用调试模式
local stmt_str = MySql.stmt_str
local stmt_value = MySql.stmt_value

-- 类型字符串转为mysql类型，必须和mysql源码类型对应
local TYPE_STR = {
    -- 在mysql_test里用data_type_test创建一个table，插入一条数据，再select出来
    -- 在C++那边就可以打印出各个类型对应枚举
    -- 这里的类型和推导出来的不太一样：比如longtext是MYSQL_TYPE_BLOB=252而不是MYSQL_TYPE_LONG_BLOB=251

    -- TODO 这里只转换了部分常用的

    DECIMAL = 0,-- MYSQL_TYPE_DECIMAL,
    tinyint = 1,-- MYSQL_TYPE_TINY,
    smallint = 2,-- MYSQL_TYPE_SHORT,
    int = 3,-- MYSQL_TYPE_LONG,
    float = 4,-- MYSQL_TYPE_FLOAT,
    double = 5,-- MYSQL_TYPE_DOUBLE,
    NULL = 6,-- MYSQL_TYPE_NULL,
    timestamp = 7,-- MYSQL_TYPE_TIMESTAMP,
    bigint = 8,-- MYSQL_TYPE_LONGLONG,
    mediumint = 9,-- MYSQL_TYPE_INT24,
    date = 10,-- MYSQL_TYPE_DATE,
    time = 11,-- MYSQL_TYPE_TIME,
    datetime = 12,-- MYSQL_TYPE_DATETIME,
    year = 13,-- MYSQL_TYPE_YEAR,
    NEWDATE = 14,-- MYSQL_TYPE_NEWDATE,
    VARCHAR = 15,-- MYSQL_TYPE_VARCHAR,
    bit = 16,-- MYSQL_TYPE_BIT,
    TIMESTAMP2 = 17,-- MYSQL_TYPE_TIMESTAMP2,
    DATETIME2 = 18,-- MYSQL_TYPE_DATETIME2,
    TIME2 = 19,-- MYSQL_TYPE_TIME2,

    json=245, --MYSQL_TYPE_JSON=245,
    decimal=246, --MYSQL_TYPE_NEWDECIMAL=246,
    enum=247, --MYSQL_TYPE_ENUM=247,
    set=248, --MYSQL_TYPE_SET=248,
    tinyblob=249, --MYSQL_TYPE_TINY_BLOB=249,
    tinytext=249,
    mediumblob=250, --MYSQL_TYPE_MEDIUM_BLOB=250,
    mediumtext=250,
    longblob=251, --MYSQL_TYPE_LONG_BLOB=251,
    longtext=251,
    blob=252, --MYSQL_TYPE_BLOB=252,
    text=252,
    varchar=253, --MYSQL_TYPE_VAR_STRING=253,
    varbinary=253,
    char=254, --MYSQL_TYPE_STRING=254,
    binary=254,
    GEOMETRY=255, --MYSQL_TYPE_GEOMETRY=255,
}

--//////////////////////////////////////////////////////////////////////////////
local C_FUNC =
{
    "thread_init",
    "thread_end",
    "error",
    "ping",
    "exec",
    "query",
    "escape",
    "disconnect",
    "stmt_clear",
    "stmt_str",
    "stmt_value",
    "stmt_exec"
}

oo.using(MySQL, MySql, function(name, func)
    return function(self, ...) return func(self.mysql, ...) end
end, C_FUNC)
--//////////////////////////////////////////////////////////////////////////////

function MySQL:__init()
    -- [tbl_name] = {a = 1, b = 2}，以表名为key，记录各个字段的数据类型
    self.types = {}
    self.mysql = MySql()
end

-- 连接数据库
-- @param host 主机名
-- @param port 端口
-- @param user 用户名
-- @param password 密码
-- @param database 数据库名
function MySQL:connect(host, port, user, password, database)
    self.database = database
    return self.mysql:connect(host, port, user, password, database)
end

-- 是否启用ssl
-- @param flag true表示启用，false表示禁用
function MySQL:set_ssl(flag)
    -- 新版本mariadb默认启用ssl，但并没有任何c函数可以关闭
    -- 关闭的方式有两种：一种是设置客户端的环境变量，另一种是服务端配置my.cnf里设置
    -- 命令行可用--skip-ssl跳过
    -- https://mariadb.com/kb/en/mariadb-connector-c-3-4-3-release-notes/
    -- If the environment variable MARIADB_TLS_DISABLE_PEER_VERIFICATION was set, the peer certificate verification will be skipped
    Util.setenv("MARIADB_TLS_DISABLE_PEER_VERIFICATION", flag and "0" or "1", true)
end

-- 读取表结构，并保存到self.types
-- @param table_name 表名
function MySQL:read_table_struct(table_name)
    local e, rows = self.mysql:query(
        string.format("SHOW FULL COLUMNS FROM %s", table_name))

    if e ~= 0 then return false end

    local types = self.types[table_name]
    if not types then
        types = {}
        self.types[table_name] = types
    end

    for _, row in ipairs(rows) do
        -- varchar(128)变为varchar
        local type_str = string.match(row.Type, "^%a+")

        types[row.Field] = assert(TYPE_STR[type_str])
    end

    return true
end

-- 读取整个数据库的表结构
function MySQL:read_database_struct()
    local e, rows = self.mysql:query("SHOW TABLES")
    if e ~= 0 then return false end

    for _, row in ipairs(rows) do
        local _, table_name = next(row)
        if not self:read_table_struct(table_name) then
            return false
        end
    end

    return true
end

-- 执行sql，无返回
-- @return 错误码
function MySQL:exec(stmt)
    return self.mysql:exec(stmt)
end

-- 执行sql，并获取返回
-- @return 错误码 结果
function MySQL:query(stmt)
    return self.mysql:query(stmt)
end

local function fetch_fields(rows)
    -- INSERT INTO table (a,b,c) VALUES (1,2,3),(4,5,6)
    -- 批量插入，必须保证每组数据的字段数量是一样的，顺序也是一样

    local fields = {}
    for k in pairs(rows[1]) do table.insert(fields, k) end

    if DEBUG and #rows > 1 then
        local fields_hash = {}
        for _, name in ipairs(fields) do fields_hash[name] = true end

        for i = 2, #rows do
            for k in pairs(rows[i]) do assert(fields_hash[k]) end
        end
    end

    return fields
end

local function stmt_wheres(mysql, types, wheres)
    if not wheres then return end

    stmt_str(mysql, " WHERE ")

    local first = true
    for k, v in pairs(wheres) do
        if first then
            first = false
            stmt_str(mysql, "`", k, "`=")
        else
            stmt_str(mysql, ",`", k, "`=")
        end
        stmt_value(mysql, types[k], v)
    end
end

-- 查询
-- @param tbl 表名
-- @param fields 查询的字段 ｛"a", "b", "c"｝，nil表示查询所有字段
-- @param wheres 条件{a=1, b=2}，只支持等于条件。其他复杂条件使用query接口
function MySQL:select(tbl, fields, wheres)
    local mysql = self.mysql

    mysql:stmt_clear()
    if fields then
        stmt_str(mysql, "SELECT (")
        for index, field in ipairs(fields) do
            if 1 == index then

                stmt_str(mysql, "`", field, "`")
            else
                stmt_str(mysql, ",`", field, "`")
            end
        end
        stmt_str(mysql, ") FROM ", tbl)
    else
        stmt_str(mysql, "SELECT * FROM ", tbl)
    end
    stmt_wheres(mysql, self.types[tbl], wheres)

    return mysql:stmt_exec(true)
end

-- 插入数据
-- @param tbl 表名
-- @param rows 数据集 {{a=1,b=2},{a=2,b=4}}
-- @param updates ON DUPLICATE KEY UPDATE {a = "values(a)", b = "values(b)"}, 只支持sql语句
function MySQL:insert(tbl, rows, updates)
    -- INSERT INTO tbl_name (a,b,c) VALUES(1,2,3),(4,5,6),(7,8,9);
    -- INSERT INTO table (a,b,c) VALUES (1,2,3),(4,5,6)  ON DUPLICATE KEY UPDATE a = VALUES(a), b = VALUES(b);

    assert(#rows > 0)
    local mysql = self.mysql
    local fields = fetch_fields(rows)

    mysql:stmt_clear()
    stmt_str(mysql, "INSERT INTO ", tbl, " (")
    for index, field in ipairs(fields) do
        if 1 == index then
            stmt_str(mysql, "`", field, "`")
        else
            stmt_str(mysql, ",`", field, "`")
        end
    end
    stmt_str(mysql, ") VALUES ")

    local types = self.types[tbl]
    for row_idx, row in pairs(rows) do
        stmt_str(mysql, 1 == row_idx and "(" or ",(")
        for index, field in ipairs(fields) do
            if 1 ~= index then stmt_str(mysql, ",") end
            stmt_value(mysql, types[field], row[field])
        end
        stmt_str(mysql, ")")
    end

    if updates then
        stmt_str(mysql, "ON DUPLICATE KEY UPDATE ")
        -- 原生的sql，ON DUPLICATE KEY UPDATE 后面的a = xxx里，xxx支持数值、字符串，二进制
        -- 但这里无法区分是直接赋值还是a = values(a)这种sql语句，因此只支持sql语句
        local first = true
        for k, v in pairs(updates) do
            if first then
                first = false
                stmt_str(mysql, "`", k, "`=", v)
            else
                stmt_str(mysql, ",`", k, "`=", v)
            end
        end
    end

    return mysql:stmt_exec(false)
end

-- 更新数据
-- @param tbl 表名
-- @param updates 更新的数据集 {a=1,b=2}
-- @param wheres 条件{a=1, b=2}，只支持等于条件。其他复杂条件使用query接口
function MySQL:update(tbl, updates, wheres)
    local mysql = self.mysql
    local types = self.types[tbl]

    mysql:stmt_clear()
    stmt_str(mysql, "UPDATE ", tbl, " SET ")

    local first = true
    for k, v in pairs(updates) do
        if first then
            first = false
            stmt_str(mysql, "`", k, "`=")
        else
            stmt_str(mysql, ",`", k, "`=")
        end
        stmt_value(mysql, types[k], v)
    end
    stmt_wheres(mysql, self.types[tbl], wheres)

    return mysql:stmt_exec(true)
end

return MySQL
