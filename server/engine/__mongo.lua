---@diagnostic disable: missing-return

-- 导出类: Mongo
Mongo = {}
--- 使用uri字符串连接到数据库
---@param uri mongodb的连接字符串
---@return 错误码
function Mongo:uriconnect(uri)
end

--- 断开连接
function Mongo:disconnect()
end

--- 设置lua table是否为数组的系数
---@param opt 整数部分表示最大key小于该值则为数组，
--- 小数部分表示数组中的元素百分比小于该值则为object，-1表示不设定
function Mongo:set_array_opt(opt)
end

--- ping服务器是否连通
---@return 错误码
function Mongo:ping()
end

--- 返回上一次错误（注意：不是每次操作都会清掉错误码）。上一次无错误则返回上上次的
---@return 错误码，错误消息
function Mongo:error()
end

--- 插入记录
---@param collection 集合名，即表名
---@param document 需要插入的内容数组，json字符串或者lua table
---@return 错误码
function Mongo:insert(collection, document)
end

--- 更新记录
---@param collection 集合名，即表名
---@param flags MONGOC_UPDATE_UPSERT等标识，按位组合
---@param query 查询条件，json字符串或者lua table
---@param update 更新的数据集，json字符串或者lua table
---@return 错误码
function Mongo:update(collection, flags, query, update)
end

--- 查询数量
---@param collection 集合名，即表名
---@param query 查询条件，json字符串或者lua table
---@param opts 查询条件，json字符串或者lua table，如{"skip":1,"limit":5}
---@return 数量，错误则返回-1
function Mongo:count(collection, query, opts)
end

--- 查询
---@param collection 集合名，即表名
---@param query 查询条件，json字符串或者lua table
---@param opts 查询条件，json字符串或者lua table
---@return 错误码，结果集
function Mongo:find(collection, query, opts)
end

--- 删除记录
---@param collection 集合名，即表名
---@params flags MONGOC_REMOVE_SINGLE_REMOVE等标识
---@param query 查询条件，json字符串或者lua table
---@return 错误码
function Mongo:remove(collection, query)
end

--- 查询记录并修改
---@param collection 集合名，即表名
---@param query 查询条件，json字符串或者lua table
---@param sort 排序条件，json字符串或者lua table
---@param update 更新条件，json字符串或者lua table
---@param fields 可选参数，控制返回结果包含的字段
---@param remvoe 是否执行删除
---@param upsert 是否执行upsert
---@param new 是否返回modify后的结果
---@return 错误码，结果集
function Mongo:find_and_modify(collection, query, sort, update, fields, remvoe, upsert, new)
end

--- 删除collection(表)
---@param collection 集合名，即表名
---@return 错误码，结果集
function Mongo:drop_collection(collection)
end

--- 删除collection的索引
---@param collection 集合名，即表名
---@pstsm index_name 索引名
---@return 错误码
function Mongo:drop_index(collection)
end

--- 创建索引
---@param collection 集合名，即表名
---@param keys 索引，json字符串或者lua table
---@param opts 索引参数，json字符串或者lua table
---@return 错误码，结果集
function Mongo:create_index(collection, keys, opts)
end

--- 执行聚合管道操作
---@param collection 集合名，即表名
---@param pipeline 管道数组，每个元素为json字符串或者lua table
---@return 错误码，结果集
function Mongo:aggregate(collection, pipeline)
end

--- 批量插入
---@param collection 集合名，即表名
---@param document 需要插入的内容数组，json字符串或者lua table
---@return 错误码
function Mongo:insert_many(collection, document)
end
