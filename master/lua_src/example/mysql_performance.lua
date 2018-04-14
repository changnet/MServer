-- mysql_performance.lua
-- 2016-03-08
-- xzc

-- mysql测试用例

--[[
mysql> CREATE USER 'test'@'%' IDENTIFIED BY 'test';
mysql> GRANT ALL PRIVILEGES ON *.* TO 'test'@'%'
    ->     WITH GRANT OPTION;

sudo vi /etc/mysql/my.cnf
#bind_address = 127.0.0.1
/etc/init.d/mysql restart

CREATE SCHEMA `mudrv` DEFAULT CHARACTER SET utf8 COLLATE utf8_general_ci ;
use mudrv;
CREATE TABLE `mudrv`.`item` (
  `auto_id` INT NOT NULL AUTO_INCREMENT,
  `id` INT NULL DEFAULT 0,
  `desc` VARCHAR(45) NULL,
  `amount` INT NULL DEFAULT 0,
  PRIMARY KEY (`auto_id`),
  UNIQUE INDEX `id_UNIQUE` (`id` ASC))
ENGINE = InnoDB
DEFAULT CHARACTER SET = utf8
COLLATE = utf8_general_ci;


if server start fail,check:
/var/log/mysql.err
/var/log/mysql.log
/var/log/syslog
]]

g_mysql_mgr   = require "mysql.mysql_mgr"
local g_mysql = g_mysql_mgr:new( "127.0.0.1",3306,"test","test","mudrv" )


local max_insert = 100000
local Mysql_performance = {}

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

f_tm_start()
Mysql_performance:insert_test()
Mysql_performance:update_test()
Mysql_performance:select_test()