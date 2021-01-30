-- mysql_test.lua
-- 2016-03-08
-- xzc

-- mysql测试用例

local g_mysql_mgr   = require "mysql.mysql_mgr"

local max_insert = 100000
local Mysql_performance = {}
local create_table_str =
"CREATE TABLE `perf_test` IF NOT EXISTS (\
  `auto_id` INT NOT NULL AUTO_INCREMENT,\
  `id` INT NULL DEFAULT 0,\
  `desc` VARCHAR(45) NULL,\
  `amount` INT NULL DEFAULT 0,\
  PRIMARY KEY (`auto_id`),\
  UNIQUE INDEX `id_UNIQUE` (`id` ASC))\
ENGINE = InnoDB\
DEFAULT CHARACTER SET = utf8\
COLLATE = utf8_general_ci;"

t_describe("sql test", function()
    local g_mysql
    t_before(function()
        local g_setting = require "setting.setting"
        g_mysql = g_mysql_mgr:new()
        g_mysql:start(g_setting.mysql_ip, g_setting.mysql_port,
            g_setting.mysql_user, g_setting.mysql_pwd,g_setting.mysql_db)

        g_mysql:exec_cmd(create_table_str)
        g_mysql:exec_cmd("TRUNCATE perf_test")
    end)

    t_it("sql base test", function()
        -- 保证
    end)

    t_after(function()
        g_mysql_mgr:stop()
    end)
end)

--[[
默认配置下：30条/s
当作一个事务，100000总共花4s
innodb_flush_log_at_trx_commit = 0,不用事务，100000总共7s
]]

function Mysql_performance:insert_test()
    print( "start mysql insert test",max_insert )
    g_mysql:exec_cmd( "START TRANSACTION" )

    -- desc是mysql关键字，因此需要加``
    for i = 1,max_insert do
        local str = string.format(
            "insert into item (id,`desc`,amount) values (%d,'%s',%d)",
            i,"just test item",i*10 )
        g_mysql:insert( str )
    end

    g_mysql:exec_cmd( "COMMIT" )
end

function Mysql_performance:update_test()
    print( "start mysql update test" )
    g_mysql:update( "update item set amount = 99999999 where id = 1" )
end

function Mysql_performance:select_test()
    print( "start mysql select test" )
    g_mysql:select( self,self.on_select_test,
        "select * from item order by amount desc limit 50" )
end

function Mysql_performance:on_select_test( ecode,res )
    print( "mysql select return ",ecode )
    vd( res )

    f_tm_stop( "mysql test done" )
end
