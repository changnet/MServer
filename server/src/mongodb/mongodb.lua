-- MongoDB读写
local MongoDB = oo.class()

local Mongo = require "engine.Mongo"

--[[
索引
https://www.mongodb.com/docs/languages/c/c-driver/current/indexes/
https://www.mongodb.com/docs/manual/reference/method/db.collection.createIndex/

Single-Field Indexes ：单字段索引，keys只有一个字段的索引
Compound Indexes： 组合索引，keys里包含多个字段
Multikey Indexes：多key索引，创建方式和单字段索引一样，只是这个字段的内容是数组
Geospatial Index：对GeoJson字段创建索引
Unique Index：在opt中指定unique字段的索引

索引值：
For an ascending index on a field, specify a value of 1. For descending index, specify a value of -1
如果是字符串索引，可以用 name = "text"
如果是geo数据，可以用2dsphere

字段名 = 索引值
local keys = {
    score = 1,
    price = 1,
    category = 1,
}

local opts = {
    unique = true, -- 唯一索引，表示keys中的组合主键只能唯一
    name = "abc", -- 索引名
}
mongodb:create_index(collection, keys, opts)

聚合管道（aggregate pipeline）
https://www.mongodb.com/docs/manual/aggregation/

MongoDB没法像SQL那样，通过SQL执行各种各样的逻辑。MongoDB提供类似的机制就是聚合管道。
聚合管理就是把复杂的操作拆分成一步步的，每一步用一个指令来表示，多个步骤合在一起就可
以完成一个复杂的操作。

db.orders.aggregate( [
   // 第一步：匹配对应的数据
   {
      $match: { size: "medium" }
   },
   // 第二步：按名字分组并计算 total quantity
   {
      $group: { _id: "$name", totalQuantity: { $sum: "$quantity" } }
   }
    // 第三步：返回前3个结果
    {$limit: 3}
] )
聚合管道需要熟悉常用的$match、$group、$sort、$limit、$skip等操作
]]

--//////////////////////////////////////////////////////////////////////////////
-- 这些定义的值要和mongo c driver(mongoc-flags.h)中的一致
-- 在C中的定义，前面都有MONGOC，如：REMOVE_NONE == MONGOC_REMOVE_NONE
MongoDB.REMOVE_NONE = 0
MongoDB.REMOVE_SINGLE_REMOVE = 1
MongoDB.UPDATE_NONE = 0
MongoDB.UPDATE_UPSERT = 1
MongoDB.UPDATE_MULTI_UPDATE = 2

local C_FUNC =
{
    "uriconnect",
    "disconnect",
    "ping",
    "error",
    "insert",
    "update",
    "count",
    "find",
    "remove",
    "set_array_opt",
    "find_and_modify",
    "drop_collection",
    "create_index",
    "drop_index",
    "aggregate",
    "insert_many",
}

oo.using(MongoDB, Mongo, function(name, func)
    return function(self, ...) return func(self.mongo, ...) end
end, C_FUNC)
--//////////////////////////////////////////////////////////////////////////////

function MongoDB:__init()
    self.mongo = Mongo()
end

function MongoDB:connect(host, port, user, passwd, database)
    -- mongodb://test_usr:test_pwd@127.0.0.1:27017/test_db
    return self.mongo:uriconnect(string.format(
        "mongodb://%s:%s@%s:%d/%s",
        user, passwd, host, port or 27017, database))
end

return MongoDB
