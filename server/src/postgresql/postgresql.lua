-- postgresql.lua
-- 2026-10-27
-- xzc

--[[
postgresql数据存储处理

与mysql.lua的差异：
1. PG能区分boolean和整数，lua里true、false可以原样读写（mysql用tinyint保存，只有数字）
2. 标识符用双引号。PG会把未加引号的标识符折叠成小写（UserData会变成userdata），
   因此这里统一加双引号，建表时用小写命名即可保持一致
3. 没有db.tbl这种跨库写法，一个连接只能落在一个database上，多库要改schema
4. 时间字符串是 2026-10-26 10:08:50.123456+08 这种格式（带时区、小数秒），
   与mysql的 YYYY-MM-DD HH:MM:SS 不同，做缓存key或者比较前要先统一
5. decimal在lua里用double表示，精度可能不够。金额建议按字符串取
6. 字符串里不接受\0，二进制必须用bytea
7. upsert是 ON CONFLICT ... DO UPDATE，不是mysql的 ON DUPLICATE KEY UPDATE，
   且必须指定冲突目标（主键或者唯一约束的列），PG没有自动匹配任意唯一键的写法
]]

-- cmake 的 ENABLE_POSTGRESQL 关闭时，C++ 侧不会注册 engine.PostgreSQL，
-- require 会直接报错，所以这个判断必须放在 require 之前
if g_sharedata:get("__ENABLE_POSTGRESQL__") ~= "true" then return end

-- C++底层绑定(engine.PostgreSQL)
local PostgreSQLBase = require "engine.PostgreSQL"

-- Lua封装类，对外暴露这个名字
local PostgreSQL = oo.class("PostgreSQL")

local DEBUG = true -- 是否启用调试模式

local stmt_str = PostgreSQLBase.stmt_str
local stmt_value = PostgreSQLBase.stmt_value

-- stmt_value的逻辑类型，必须和postgresql.hpp里的SqlValueType一致
local VT = {
    INT = 1,      -- smallint、integer、bigint
    FLOAT = 2,    -- real、double precision、numeric
    STRING = 3,   -- char、varchar、text、uuid、json
    DATETIME = 4, -- date、time、timestamp
    BLOB = 5,     -- bytea
    BOOL = 6,     -- boolean
}

-- ON CONFLICT DO UPDATE里引用待插入值的标记。
-- {amount = PostgreSQL.EXCLUDED} 会生成 "amount" = EXCLUDED."amount"，
-- 用于 b = EXCLUDED.b + 1 这类需求
PostgreSQL.EXCLUDED = {}
local EXCLUDED = PostgreSQL.EXCLUDED

-- information_schema.columns里的udt_name -> 逻辑类型
-- 用udt_name而不是data_type，是因为data_type带修饰（如 "character varying"），
-- udt_name才是内部类型名（varchar、timestamptz这种）
local UDT_TYPE = {
    bool = VT.BOOL,

    int2 = VT.INT,
    int4 = VT.INT,
    int8 = VT.INT,

    float4 = VT.FLOAT,
    float8 = VT.FLOAT,
    numeric = VT.FLOAT,
    money = VT.FLOAT,

    -- char(n)是bpchar，varchar是varchar，两者查出来都是字符串
    bpchar = VT.STRING,
    varchar = VT.STRING,
    text = VT.STRING,
    name = VT.STRING,
    uuid = VT.STRING,
    json = VT.STRING,
    jsonb = VT.STRING,
    xml = VT.STRING,

    date = VT.DATETIME,
    time = VT.DATETIME,
    timetz = VT.DATETIME,
    timestamp = VT.DATETIME,
    timestamptz = VT.DATETIME,

    bytea = VT.BLOB,
}

--//////////////////////////////////////////////////////////////////////////////
local C_FUNC = {
    "connect",
    "connect_uri",
    "error",
    "ping",
    "exec",
    "query",
    "escape",
    "escape_literal",
    "escape_identifier",
    "escape_bytea",
    "unescape_bytea",
    "affected_rows",
    "set_notice_log",
    "disconnect",
    "stmt_clear",
    "stmt_str",
    "stmt_value",
    "stmt_exec",
}

oo.using(PostgreSQL, PostgreSQLBase, function(name, func)
    return function(self, ...) return func(self.postgresql, ...) end
end, C_FUNC)
--//////////////////////////////////////////////////////////////////////////////

function PostgreSQL:__init()
    -- [tbl_name] = {field_name = 逻辑类型}，以表名为key，记录各个字段的类型
    self.types = {}
    -- [tbl_name] = {field_name = 原始行信息}，含is_nullable、column_default
    self.fields = {}
    self.postgresql = PostgreSQLBase()
    self.connected = false
end

-- 连接数据库
-- @param host 主机名
-- @param port 端口
-- @param user 用户名
-- @param password 密码
-- @param database 数据库名
function PostgreSQL:connect(host, port, user, password, database)
    self.database = database
    return self.postgresql:connect(host, port, user, password, database)
end

-- 按优先级连接数据库，连接不上会一直重试
-- @param host 主机名
-- @param port 端口
-- @param user 用户名
-- @param password 密码
-- @param database 数据库名
-- @param sslmode 可选，ssl模式，如 "disable"、"require"
-- @param callback 连接成功回调，参数为PostgreSQL对象
function PostgreSQL:connect_later(host, port, user, password, database, sslmode,
                                 callback)
    local name = "PostgreSQL:" .. database

    -- 用conninfo是为了支持sslmode、target_session_attrs这些参数。
    -- 注意conninfo是kv格式，密码里不能有空格（含空格要写成 password='a b'），
    -- 这种情况请用connect接口
    local conninfo = string.format(
        "host=%s port=%d user=%s password=%s dbname=%s connect_timeout=60",
        host, port, user, password, database)
    if sslmode then
        conninfo = conninfo .. " sslmode=" .. sslmode
    end

    Startup.reg(function(retry)
        if not retry then
            local e = self.postgresql:connect_uri(conninfo)
            if 0 ~= e then
                local e1, str = self.postgresql:error()
                printf("%s(%d): %s", name, e1, str)
                return false
            end
            -- 立马用ping确认结果并设置connected
        end
        if 0 == self.postgresql:ping() then
            self.connected = true
            if callback then callback(self) end
            return true
        end

        local e, str = self.postgresql:error()
        printf("%s(%d): %s", name, e, str)
        return false
    end)
end

-- 标识符(表名、字段名)统一转加双引号的形式
-- @param name 表名或者字段名
function PostgreSQL:ident(name)
    return self.postgresql:escape_identifier(name)
end

-- 读取表结构，并保存到self.types
-- @param tbl 表名
function PostgreSQL:read_table_struct(tbl)
    local pg = self.postgresql

    local e, rows = pg:query(string.format(
        "SELECT column_name,data_type,udt_name,is_nullable,column_default"
            .. " FROM information_schema.columns WHERE table_schema = 'public'"
            .. " AND table_name = %s ORDER BY ordinal_position",
        pg:escape_literal(tbl)))

    if 0 ~= e then return false end

    local types = {}
    local fields = {}
    for _, row in ipairs(rows) do
        local field = row.column_name
        fields[field] = row

        local vtype = UDT_TYPE[row.udt_name]
        if not vtype then
            -- enum、数组、自定义类型的udt_name不固定，按字符串处理并打日志
            printf("postgresql read_table_struct %s.%s unknow type %s(%s)",
                tbl, field, tostring(row.udt_name), tostring(row.data_type))
            vtype = VT.STRING
        end
        types[field] = vtype
        row.vtype = vtype
    end

    self.types[tbl] = types
    self.fields[tbl] = fields

    return true
end

-- 读取整个数据库的表结构
function PostgreSQL:read_database_struct()
    -- information_schema.tables 是PG的系统视图，没有 SHOW TABLES 这种写法
    local e, rows = self.postgresql:query(
        "SELECT table_name FROM information_schema.tables"
            .. " WHERE table_schema = 'public' AND table_type = 'BASE TABLE'")
    if 0 ~= e then return false end

    for _, row in ipairs(rows) do
        if not self:read_table_struct(row.table_name) then return false end
    end

    return true
end

-- 执行sql，无返回
-- @return 错误码
function PostgreSQL:exec(stmt)
    return self.postgresql:exec(stmt)
end

-- 执行sql，并获取返回
-- @return 错误码 结果
function PostgreSQL:query(stmt)
    return self.postgresql:query(stmt)
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

local function stmt_wheres(pg, types, wheres)
    if not wheres then return end

    stmt_str(pg, " WHERE ")

    local first = true
    for k, v in pairs(wheres) do
        stmt_str(pg, first and "" or " AND ", pg:escape_identifier(k), "=")
        first = false
        stmt_value(pg, types[k], v)
    end
end

-- ON CONFLICT 子句。PG的upsert必须指定冲突目标，没有自动匹配任意唯一键的写法
-- @param conflict 冲突目标，字符串原样使用，如 "(id)"、"ON CONSTRAINT uniq_id"；
-- 数组则按字段名生成，如 {"id"} -> ("id")
local function stmt_conflict(pg, types, updates, conflict)
    if not conflict then return end

    if "table" == type(conflict) then
        local list = {}
        for index, name in ipairs(conflict) do
            list[index] = pg:escape_identifier(name)
        end
        conflict = string.format("(%s)", table.concat(list, ","))
    end

    stmt_str(pg, " ON CONFLICT ", conflict)

    if not updates then
        stmt_str(pg, " DO NOTHING")
        return
    end

    stmt_str(pg, " DO UPDATE SET ")
    local first = true
    for k, v in pairs(updates) do
        stmt_str(pg, first and "" or ",", pg:escape_identifier(k), "=")
        first = false

        if v == EXCLUDED then
            stmt_str(pg, "EXCLUDED.", pg:escape_identifier(k))
        else
            -- 普通值走转义，不用手写EXCLUDED语法，也不接受sql片段（防注入）
            stmt_value(pg, types[k], v)
        end
    end
end

-- 查询
-- @param tbl 表名
-- @param fields 查询的字段 {"a", "b", "c"}，nil表示查询所有字段
-- @param wheres 条件{a=1, b=2}，只支持等于条件。其他复杂条件使用query接口
function PostgreSQL:select(tbl, fields, wheres)
    local pg = self.postgresql

    pg:stmt_clear()
    if fields then
        stmt_str(pg, "SELECT ")
        for index, field in ipairs(fields) do
            stmt_str(pg, 1 == index and "" or ",", pg:escape_identifier(field))
        end
        stmt_str(pg, " FROM ", pg:escape_identifier(tbl))
    else
        stmt_str(pg, "SELECT * FROM ", pg:escape_identifier(tbl))
    end
    stmt_wheres(pg, self.types[tbl], wheres)

    return pg:stmt_exec(true)
end

-- 插入数据
-- @param tbl 表名
-- @param rows 数据集 {{a=1,b=2},{a=2,b=4}}，需要保证所有数据字段名一致
-- @param updates ON CONFLICT DO UPDATE {a = 1, b = PostgreSQL.EXCLUDED}
-- @param conflict 冲突目标，必填时才生成ON CONFLICT子句，见stmt_conflict
function PostgreSQL:insert(tbl, rows, updates, conflict)
    -- INSERT INTO "tbl" ("a","b") VALUES (1,2),(3,4);
    -- INSERT INTO "tbl" ("a","b") VALUES (1,2)
    --     ON CONFLICT ("a") DO UPDATE SET "b" = EXCLUDED."b";

    assert(#rows > 0)
    local pg = self.postgresql
    local fields = fetch_fields(rows)
    local types = self.types[tbl]

    pg:stmt_clear()
    stmt_str(pg, "INSERT INTO ", pg:escape_identifier(tbl), " (")
    for index, field in ipairs(fields) do
        stmt_str(pg, 1 == index and "" or ",", pg:escape_identifier(field))
    end
    stmt_str(pg, ") VALUES ")

    for row_idx, row in ipairs(rows) do
        stmt_str(pg, 1 == row_idx and "(" or ",(")
        for index, field in ipairs(fields) do
            if 1 ~= index then stmt_str(pg, ",") end
            stmt_value(pg, types[field], row[field])
        end
        stmt_str(pg, ")")
    end

    stmt_conflict(pg, types, updates, conflict)

    return pg:stmt_exec(false)
end

-- 更新数据
-- @param tbl 表名
-- @param updates 更新的数据集 {a=1,b=2}
-- @param wheres 条件{a=1, b=2}，只支持等于条件。其他复杂条件使用query接口
function PostgreSQL:update(tbl, updates, wheres)
    local pg = self.postgresql
    local types = self.types[tbl]

    pg:stmt_clear()
    stmt_str(pg, "UPDATE ", pg:escape_identifier(tbl), " SET ")

    local first = true
    for k, v in pairs(updates) do
        stmt_str(pg, first and "" or ",", pg:escape_identifier(k), "=")
        first = false
        stmt_value(pg, types[k], v)
    end
    stmt_wheres(pg, types, wheres)

    return pg:stmt_exec(false)
end

-- 删除数据（mysql.lua没有这个接口，PG版补上）
-- @param tbl 表名
-- @param wheres 条件{a=1, b=2}，只支持等于条件。其他复杂条件使用query接口
function PostgreSQL:delete(tbl, wheres)
    local pg = self.postgresql

    pg:stmt_clear()
    stmt_str(pg, "DELETE FROM ", pg:escape_identifier(tbl))
    stmt_wheres(pg, self.types[tbl], wheres)

    return pg:stmt_exec(false)
end

-- 创建索引
-- @param tbl 表名
-- @param name 索引名
-- @param fields 字段名数组 {"a", "b"}
-- @param opts 可选 {unique = true, concurrently = true, if_not_exists = true}
-- CONCURRENTLY不锁表，适合线上，但它不能在事务块里执行，这里单独下发一条sql
function PostgreSQL:create_index(tbl, name, fields, opts)
    opts = opts or {}
    -- PG不支持 CREATE INDEX CONCURRENTLY IF NOT EXISTS，这两个不能同时用
    assert(not (opts.concurrently and opts.if_not_exists),
        "postgresql: CONCURRENTLY 不支持 IF NOT EXISTS")

    local pg = self.postgresql

    local list = {"CREATE "}
    if opts.unique then list[#list + 1] = "UNIQUE " end
    list[#list + 1] = "INDEX "
    if opts.concurrently then list[#list + 1] = "CONCURRENTLY " end
    if opts.if_not_exists then list[#list + 1] = "IF NOT EXISTS " end
    list[#list + 1] = pg:escape_identifier(name)
    list[#list + 1] = " ON "
    list[#list + 1] = pg:escape_identifier(tbl)
    list[#list + 1] = " ("
    for index, field in ipairs(fields) do
        list[#list + 1] = (1 == index and "" or ",") .. pg:escape_identifier(field)
    end
    list[#list + 1] = ")"

    return pg:exec(table.concat(list))
end

return PostgreSQL
