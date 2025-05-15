-- mysql_test.lua
-- 2016-03-08
-- xzc
-- mysql测试用例
local MySQL = require "mysql.mysql"

local max_insert = 10000

-- desc字段和mysql关键字一样是故意的，用来测试转义是否生效
local create_table_str = "CREATE TABLE IF NOT EXISTS `perf_test` (\
  `auto_id` INT NOT NULL AUTO_INCREMENT,\
  `id` INT NULL DEFAULT 0,\
  `desc` TEXT NULL,\
  `amount` INT NULL DEFAULT 0,\
  PRIMARY KEY (`auto_id`),\
  UNIQUE INDEX `id_UNIQUE` (`id` ASC))\
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
        -- mysql:exec_cmd("TRUNCATE perf_test")
        -- Test.equal(mysql:exec("DROP TABLE IF EXISTS perf_test"), 0)
        -- Test.equal(mysql:exec(create_table_str), 0)
    end)
--[[
    Test.it("mysql base test", function()
        Test.equal(mysql:exec("delete from perf_test"), 0)

        -- 插入
        Test.equal(mysql:insert("perf_test", {
            id = 1,
            desc = 'base test item >>>>>>>>>>>>>>>>>>>><<<<<<<<<<<<<',
            amount = 1
        }), 0)
        Test.equal(mysql:insert("perf_test", {
            id = 2,
            desc = 'base test item >>>>>>>>>>>>>>>>>>>><<<<<<<<<<<<<',
            amount = 9
        }), 0)
        Test.equal(mysql:insert("perf_test", {
            id = 2,
            desc = 'base test item >>>>>>>>>>>>>>>>>>>><<<<<<<<<<<<<',
            amount = 2
        }, {amount = "values(amount)"}), 0)

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
    end)

    local n_insert = 500 -- 太多了完成不了测试的
    Test.it(string.format("sql perf %d insert", n_insert), function()
        -- 默认配置下：30~100条/s
        -- 当作一个事务插入，100000总共花4s
        -- 启用innodb_flush_log_at_trx_commit = 0,不用事务，100000总共7s

        Test.equal(mysql:exec("delete from perf_test"), 0)

        local row = {desc = "just test itemmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm"}
        local rows = {row}
        for i = 1, max_insert do
            row.id = i
            row.amount = i * 10
            mysql:insert("perf_test", rows)
        end

        mysql:select("select * from perf_test", function(ecode, res)
            Test.equal(ecode, 0)
            Test.equal(#res, n_insert)
            Test.done()
        end)
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
        mysql:disconnect()
    end)]]
end)
