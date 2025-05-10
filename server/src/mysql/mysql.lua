-- mysql.lua
-- 2015-11-20
-- xzc

-- mysql数据存储处理

local MySql = require "engine.MySql"


local MySQL = oo.class()

function MySQL:__init()
    -- [tbl_name] = {a = 1, b = 2}，以表名为key，记录各个字段的数据类型
    self.types = {}
    self.mysql = MySql()
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

local function fetch_fields(values)
end

local function stmt_wheres(mysql, types, wheres)
    if not wheres then return end

    mysql:stmt_str(" WHERE ")

    local first = true
    for k, v in pairs(wheres) do
        if first then
            first = false
            mysql:stmt_str(k, "=")
        else
            mysql:stmt_str(",", k, "=")
        end
        mysql:stmt_value(types[k], v)
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
        mysql:stmt_str("SELECT")
        mysql:stmt_str(" FROM ", tbl)
    else
        mysql:stmt_str("SELECT * FROM ", tbl)
    end
    stmt_wheres(mysql, self.types, wheres)

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
    mysql:stmt_str("INSERT INTO ", tbl, "(")
    for index, field in ipairs(fields) do
        if 1 == index then
            mysql:stmt_str(field)
        else
            mysql:stmt_str(",", field)
        end
    end
    mysql:stmt_str("VALUES ")

    local types = self.types
    for row_idx, row in pairs(rows) do
        mysql:stmt_str(1 == row_idx and "(" or ",(")
        for index, field in ipairs(fields) do
            if 1 == index then mysql:stmt_str(",") end
            mysql:stmt_value(types[field], row[field])
        end
        mysql:stmt_str(")")
    end

    if updates then
        mysql:stmt_str("ON DUPLICATE KEY UPDATE ")
        -- 原生的sql，ON DUPLICATE KEY UPDATE 后面的a = xxx里，xxx支持数值、字符串，二进制
        -- 但这里无法区分是直接赋值还是a = values(a)这种sql语句，因此只支持sql语句
        local first = true
        for k, v in pairs(updates) do
            if first then
                first = false
                mysql:stmt_str(k, "=", v)
            else
                mysql:stmt_str(",", k, "=", v)
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
    local types = self.types

    mysql:stmt_clear()
    mysql:stmt_str("UPDATE ", tbl, "SET ")

    local first = true
    for k, v in pairs(updates) do
        if first then
            first = false
            mysql:stmt_str(k, "=")
        else
            mysql:stmt_str(",", k, "=")
        end
        mysql:stmt_value(types[k], v)
    end
    stmt_wheres(mysql, self.types, wheres)

    return mysql:stmt_exec(true)
end

return MySQL
