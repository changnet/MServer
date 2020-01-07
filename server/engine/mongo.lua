-- Mongo
-- auto export by engine_api.lua do NOT modify!

-- MongoDB 数据库操作
Mongo = {}

-- 启动子线程并连接数据库，注：该操作是异步的
-- @param host 数据库地址
-- @param port 数据库端口
-- @param usr  登录用户名
-- @param pwd  登录密码
-- @param dbname 数据库名
function Mongo:start(host, port, usr, pwd, dbname)
end

-- 停止子线程，并待子线程退出。该操作会把线程中的任务全部完成后再退出，但不会
-- 等待主线程取回结果
function Mongo:stop()
end

-- 校验当前与数据库连接是否成功，注：不校验因网络问题暂时断开的情况
-- @return boolean
function Mongo:valid()
end

-- 查询
-- @param id 唯一id，查询结果根据此id回调
-- @param collection 集合名，即表名
-- @param query 查询条件，json字符串或者lua table
-- @param fields 需要查询的字段，json字符串或者lua table
function Mongo:find(id, collection, query, fields)
end

-- 查询数量
-- @param id 唯一id，查询结果根据此id回调
-- @param collection 集合名，即表名
-- @param query 查询条件，json字符串
-- @param opts 查询条件，json字符串
function Mongo:count(id, collection, query, opts)
end

-- 插入记录
-- @param id 唯一id，查询结果根据此id回调
-- @param collection 集合名，即表名
-- @param ctx 需要插入的内容，json字符串或者lua table
function Mongo:insert(id, collection, ctx)
end

-- 更新记录
-- @param id 唯一id，查询结果根据此id回调
-- @param collection 集合名，即表名
-- @param query 查询条件，json字符串或者lua table
-- @param upsert 如果不存在，则插入
-- @param multi 旧版本参数，已废弃
function Mongo:update(id, collection, query, upsert, multi)
end

-- 删除记录
-- @param id 唯一id，查询结果根据此id回调
-- @param collection 集合名，即表名
-- @param query 查询条件，json字符串或者lua table
-- @param multi 旧版本参数，已废弃
function Mongo:remove(id, collection, query, multi)
end

-- 查询记录并修改
-- @param id 唯一id，查询结果根据此id回调
-- @param collection 集合名，即表名
-- @param query 查询条件，json字符串或者lua table
-- id,collection,query,sort,update,fields,remove,upsert,new
-- TODO: 这个接口参数不对了，后面统一更新
function Mongo:find_and_modify(id, collection, query)
end

