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

local Store_sql   = require "sql.store_sql"
g_store_sql   = Store_sql()

g_store_sql:start( "127.0.0.1",3306,"test","test","mudrv" )


local max_insert = 10

print( "insert mysql",max_insert )

-- desc是mysql关键字，因此需要加``
for i = 1,max_insert do
    local str = string.format( "insert into item (id,`desc`,amount) values (%d,'%s',%d)",
        i,"just test item",i*10 )
    g_store_sql:do_sql( str )
end

local str = "update item set amount = 99999999 where id = 1"
g_store_sql:do_sql( str )

str = "select * from item order by amount desc limit 50"
g_store_sql:do_sql( str,function( err,result )
        print( "mysql select return ",err )
        vd( result )
    end
    )
