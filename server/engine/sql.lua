-- Sql
-- auto export by engine_api.lua do NOT modify!

-- MySQL、MariaDB 操作
Sql = {}

-- 校验当前与数据库连接是否成功，注：不校验因网络问题暂时断开的情况
-- @return boolean
function Sql:valid()
end

-- 启动子线程并连接数据库，注：该操作是异步的
-- @param host 数据库地址
-- @param port 数据库端口
-- @param usr  登录用户名
-- @param pwd  登录密码
-- @param dbname 数据库名
function Sql:start(host, port, usr, pwd, dbname)
end

-- 停止子线程，并待子线程退出。该操作会把线程中的任务全部完成后再退出但不会
-- 等待主线程取回结果
function Sql:stop()
end

-- 执行sql语句
-- @param id 唯一Id，执行完会根据此id回调到脚本。如果为0，则不回调
-- @param stmt sql语句
function Sql:do_sql(id, stmt)
end

