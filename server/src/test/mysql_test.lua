-- mysql_test.lua
-- 2016-03-08
-- xzc
-- mysql测试用例
local MySQL = require "mysql.mysql"

local max_insert = 10000

-- desc字段和mysql关键字一样是故意的，用来测试转义是否生效
local perf_test_stmt = "CREATE TABLE IF NOT EXISTS `perf_test` (\
  `auto_id` INT NOT NULL AUTO_INCREMENT,\
  `id` INT NULL DEFAULT 0,\
  `desc` TEXT NULL,\
  `amount` INT NULL DEFAULT 0,\
  PRIMARY KEY (`auto_id`),\
  UNIQUE INDEX `id_UNIQUE` (`id` ASC))\
ENGINE = InnoDB\
DEFAULT CHARACTER SET = utf8\
COLLATE = utf8_general_ci;"

local data_type_test_stmt = "CREATE TABLE IF NOT EXISTS `data_type_test` (\
  `id` INT NOT NULL AUTO_INCREMENT,\
  `tiny_int_field` TINYINT,\
  `small_int_field` SMALLINT,\
  `medium_int_field` MEDIUMINT,\
  `int_field` INT,\
  `big_int_field` BIGINT,\
  `float_field` FLOAT,\
  `double_field` DOUBLE,\
  `decimal_field` DECIMAL(10,2),\
  `char_field` CHAR(10),\
  `varchar_field` VARCHAR(255),\
  `tiny_text_field` TINYTEXT,\
  `text_field` TEXT,\
  `medium_text_field` MEDIUMTEXT,\
  `long_text_field` LONGTEXT,\
  `binary_field` BINARY(10),\
  `varbinary_field` VARBINARY(255),\
  `tiny_blob_field` TINYBLOB,\
  `blob_field` BLOB,\
  `medium_blob_field` MEDIUMBLOB,\
  `long_blob_field` LONGBLOB,\
  `date_field` DATE,\
  `time_field` TIME,\
  `datetime_field` DATETIME,\
  `timestamp_field` TIMESTAMP,\
  `year_field` YEAR,\
  `enum_field` ENUM('small', 'medium', 'large'),\
  `set_field` SET('red', 'green', 'blue'),\
  `json_field` JSON,\
  `boolean_field` BOOLEAN,\
  PRIMARY KEY (`id`))\
ENGINE = InnoDB\
DEFAULT CHARACTER SET = utf8\
COLLATE = utf8_general_ci;"

Test.describe("mysql test", function()
    local mysql
    Test.before(function()
        mysql = MySQL()

        mysql:thread_init()
        mysql:set_ssl(false)
        local e = mysql:connect(g_setting.mysql_ip, g_setting.mysql_port,
                    g_setting.mysql_user, g_setting.mysql_pwd,
                    g_setting.mysql_db)
        if 0 ~= e then
            print("mysql connect error", mysql:error())
            Test.assert(false)
        end

        -- TODO TRUNCATE和DROP TABLE会卡3秒左右，不太清楚为什么
        -- mysql:exec("TRUNCATE perf_test")
        Test.equal(mysql:exec("DROP TABLE IF EXISTS perf_test"), 0)
        Test.equal(mysql:exec("DROP TABLE IF EXISTS data_type_test"), 0)
        Test.equal(mysql:exec(perf_test_stmt), 0)
        Test.equal(mysql:exec(data_type_test_stmt), 0)
        Test.assert(mysql:read_database_struct())
        mysql:select("data_type_test2")
        assert(false)
    end)

    Test.it("mysql base test", function()
        Test.equal(mysql:exec("delete from perf_test"), 0)
        Test.equal(mysql:exec("delete from data_type_test"), 0)

        -- 插入
        Test.equal(mysql:insert("perf_test", {{
            id = 1,
            desc = 'base test item >>>>>>>>>>>>>>>>>>>><<<<<<<<<<<<<',
            amount = 1
        }}), 0)
        Test.equal(mysql:insert("perf_test", {{
            id = 2,
            desc = 'base test item >>>>>>>>>>>>>>>>>>>><<<<<<<<<<<<<',
            amount = 9
        }}), 0)
        Test.equal(mysql:insert("perf_test", {{
            id = 2,
            desc = 'base test item >>>>>>>>>>>>>>>>>>>><<<<<<<<<<<<<',
            amount = 2
        }}, {amount = "values(amount)"}), 0)

        -- 更新超长字符串，测试底层缓冲区分配
        local desc = string.rep("longgggggggggggg", 512)
        -- 更新
        Test.equal(mysql:update("perf_test",
            {amount = 99999, desc = desc}, {id = 1}), 0)

        local e, rows = mysql:select("perf_test", {"amount"}, {id = 2})
        Test.equal(e, 0)
        Test.equal(#rows, 1)
        Test.equal(rows[1].amount, 2) -- 校验上面的on duplicate update生效

        -- 删除
        Test.equal(mysql:exec("delete from perf_test where id = 2"), 0)
        -- 查询
        e, rows = mysql:select("perf_test")
        Test.equal(e, 0)
        Test.equal(#rows, 1)
        Test.equal(rows[1].id, 1)
        Test.equal(rows[1].amount, 99999)
        Test.equal(rows[1].desc, desc)

        -- mysql:select("data_type_test")
        -- 测试所有字段类型
        Test.equal(mysql:exec("delete from data_type_test"), 0)
        local row = {
            id = 1,
            tiny_int_field = 127,                    -- TINYINT: -128 to 127
            small_int_field = 32767,                 -- SMALLINT: -32768 to 32767
            medium_int_field = 8388607,              -- MEDIUMINT: -8388608 to 8388607
            int_field = 2147483647,                  -- INT: -2147483648 to 2147483647
            big_int_field = 9223372036854775807,     -- BIGINT: -9223372036854775808 to 9223372036854775807
            float_field = 3.14159,                   -- FLOAT: Single precision
            double_field = 3.141592653589793,        -- DOUBLE: Double precision
            decimal_field = 12345678.90,             -- DECIMAL(10,2): Fixed point
            char_field = "fixed10",                  -- CHAR(10): Fixed length string
            varchar_field = "variable length string", -- VARCHAR(255): Variable length string
            tiny_text_field = "tiny text content",   -- TINYTEXT: Up to 255 bytes
            text_field = "regular text content",     -- TEXT: Up to 65KB
            medium_text_field = "medium text content", -- MEDIUMTEXT: Up to 16MB
            long_text_field = "long text content",   -- LONGTEXT: Up to 4GB
            binary_field = "bbbbbbbbbb",                 -- BINARY(10): Fixed length binary
            varbinary_field = "varbinary data",      -- VARBINARY(255): Variable length binary
            tiny_blob_field = "tiny blob",           -- TINYBLOB: Up to 255 bytes
            blob_field = "blob data",                -- BLOB: Up to 65KB
            medium_blob_field = "medium blob",       -- MEDIUMBLOB: Up to 16MB
            long_blob_field = "long blob",           -- LONGBLOB: Up to 4GB
            date_field = "2024-03-20",               -- DATE: YYYY-MM-DD
            time_field = "14:30:00",                 -- TIME: HH:MM:SS
            datetime_field = "2024-03-20 14:30:00",  -- DATETIME: YYYY-MM-DD HH:MM:SS
            timestamp_field = "2024-03-20 14:30:00", -- TIMESTAMP: YYYY-MM-DD HH:MM:SS
            year_field = 2024,                       -- YEAR: YYYY
            enum_field = "medium",                   -- ENUM: 'small', 'medium', 'large'
            set_field = "red,blue",                  -- SET: 'red', 'green', 'blue'
            json_field = '{"name": "test", "value": 123}', -- JSON: JSON data
            -- mysql虽然支持boolean，但是以tinyint来保存的，select出来也是1和0
            -- 因此这里是不支持true和flase值，只支持数字
            boolean_field = 1 ,                    -- BOOLEAN: TRUE/FALSE
        }
        Test.equal(mysql:insert("data_type_test", {row}), 0)
        e, rows = mysql:select("data_type_test")
        Test.equal(e, 0)
        Test.equal(#rows, 1)
        Test.equal(rows[1], row)
    end)

    local n_insert = 500 -- 太多了完成不了测试的
    Test.it(string.format("sql perf %d insert", n_insert), function()
        -- 默认配置下：30~100条/s
        -- 当作一个事务插入，100000总共花4s
        -- 启用innodb_flush_log_at_trx_commit = 0,不用事务，100000总共7s

        Test.equal(mysql:exec("delete from perf_test"), 0)

        local row = {desc = "just test itemmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm"}
        local rows = {row}
        for i = 1, n_insert do
            row.id = i
            row.amount = i * 10
            mysql:insert("perf_test", rows)
        end

        local e
        e, rows = mysql:select("perf_test")
        Test.equal(e, 0)
        Test.equal(#rows, n_insert)
    end)

    Test.it(string.format("mysql perf %d insert transaction mode", max_insert),
        function()

        Test.equal(mysql:exec("delete from perf_test"), 0)
        mysql:exec("START TRANSACTION")

        local row = {desc = "just test itemmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm"}
        local rows = {row}
        for i = 1, max_insert do
            row.id = i
            row.amount = i * 10
            mysql:insert("perf_test", rows)
        end

        mysql:exec("COMMIT")

        local e
        e, rows = mysql:select("perf_test")
        Test.equal(e, 0)
        Test.equal(#rows, max_insert)
    end)
    Test.it(string.format("mysql perf %d insert batch mode", max_insert),
        function()

        Test.equal(mysql:exec("delete from perf_test"), 0)

        local rows = {}
        for i = 1, max_insert do
            table.insert(rows, {
                id = i,
                amount = i * 10,
                desc = "just test itemmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm"
            })
        end

        mysql:insert("perf_test", rows)

        local e
        e, rows = mysql:select("perf_test")
        Test.equal(e, 0)
        Test.equal(#rows, max_insert)
    end)

    Test.after(function()
        mysql:exec("DROP TABLE IF EXISTS perf_test")
        -- mysql:exec("DROP TABLE IF EXISTS data_type_test")
        mysql:disconnect()
    end)
end)
