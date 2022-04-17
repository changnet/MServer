-- mysql_test.lua
-- 2016-03-08
-- xzc
-- mysql测试用例
local Mysql = require "mysql.mysql"

local max_insert = 10000

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

t_describe("sql test", function()
    local mysql
    t_it("sql base test", function()
        t_async(10000)

        mysql = Mysql("test_mysql")
        local g_setting = require "setting.setting"
        mysql:start(g_setting.mysql_ip, g_setting.mysql_port,
                    g_setting.mysql_user, g_setting.mysql_pwd,
                    g_setting.mysql_db, function()
            t_print(string.format("mysql(%s:%d) ready ...", g_setting.mysql_ip,
                                  g_setting.mysql_port))

            -- TODO TRUNCATE和DROP TABLE会卡3秒左右，不太清楚为什么
            -- mysql:exec_cmd("TRUNCATE perf_test")
            mysql:exec_cmd("DROP TABLE IF EXISTS perf_test")
            mysql:exec_cmd(create_table_str)
            mysql:exec_cmd("delete from perf_test")

            -- 插入
            mysql:insert("insert into perf_test (id,`desc`,amount) values \z
                    (1,'base test item >>>>>>>>>>>>>>>>>>>><<<<<<<<<<<<<',1)")
            mysql:insert("insert into perf_test (id,`desc`,amount) values \z
                    (2,'base test item >>>>>>>>>>>>>>>>>>>><<<<<<<<<<<<<',1)")
            -- 更新
            mysql:update("update perf_test set amount = 99999 where id = 1")
            -- 更新超长字符串，底层有特殊处理
            local desc = string.rep("longgggggggggggg", 512)
            mysql:update(string.format(
                             'update perf_test set `desc` = "%s" where id = 1',
                             desc))
            -- 删除
            mysql:exec_cmd("delete from perf_test where id = 2")
            -- 查询
            mysql:select("select * from perf_test", function(ecode, res)
                t_equal(ecode, 0)
                t_equal(#res, 1)
                t_equal(res[1].id, 1)
                t_equal(res[1].amount, 99999)
                t_equal(res[1].desc, desc)
            end)
            mysql:insert("insert into perf_test (id,`desc`,amount) values \z
                (3,'base test item >>>>>>>>>>>>>>>>>>>><<<<<<<<<<<<<',1)")

            mysql:exec_cmd("delete from perf_test")
            mysql:select("select * from perf_test", function(ecode, res)
                t_equal(ecode, 0)
                t_equal(res, nil) -- 当查询结果为空时，res为nil
                t_done()
            end)
        end)
    end)

    t_it(string.format("sql perf %d insert transaction mode", max_insert),
         function()
        t_async(20000)
        mysql:exec_cmd("START TRANSACTION")

        -- desc是mysql关键字，因此需要加``
        for i = 1, max_insert do
            mysql:insert(string.format(
                             "insert into perf_test (id,`desc`,amount) values \z
                    (%d,'%s',%d)", i,
                             "just test itemmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm",
                             i * 10))
        end

        mysql:exec_cmd("COMMIT")
        mysql:select("select * from perf_test", function(ecode, res)
            t_equal(ecode, 0)
            t_equal(#res, max_insert)
            t_done()
        end)
    end)

    -- 用来保证下一个测试时表是空的
    t_it("sql delete", function()
        t_async(20000)
        mysql:exec_cmd("delete from perf_test")
        mysql:select("select * from perf_test", function(ecode, res)
            t_equal(ecode, 0)
            t_equal(res, nil)
            t_done()
        end)
    end)

    --[[
    默认配置下：30~100条/s
    当作一个事务插入，100000总共花4s
    启用innodb_flush_log_at_trx_commit = 0,不用事务，100000总共7s
    ]]
    local n_insert = max_insert / 20 -- 太多了完成不了测试的
    t_it(string.format("sql perf %d insert", n_insert), function()
        t_async(20000)

        -- desc是mysql关键字，因此需要加``
        for i = 1, n_insert do
            mysql:insert(string.format(
                             "insert into perf_test (id,`desc`,amount) values \z
                    (%d,'%s',%d)", max_insert + i,
                             "just test itemmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm",
                             i * 10))
        end

        mysql:select("select * from perf_test", function(ecode, res)
            t_equal(ecode, 0)
            t_equal(#res, n_insert)
            t_done()
        end)
    end)

    t_after(function()
        mysql:stop()
    end)
end)
