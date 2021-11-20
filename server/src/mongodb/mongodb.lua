-- mongodb.lua
-- 2015-12-05
-- xzc


-- mongodb 接口
local MongoDBInterface = oo.class(...)

local Mongo = require "engine.Mongo"
local AutoId = require "modules.system.auto_id"
local MongodbMgr = require "mongodb.MongodbMgr"

function MongoDBInterface:__init()
    self.id = MongodbMgr.next_id()
    self.auto_id = AutoId()
    self.mongodb = Mongo(self.id)

    self.cb = {}
end

-- 连接成功回调
function MongoDBInterface:on_ready()
    if self.on_ready then self.on_ready() end
end

function MongoDBInterface:on_data(qid, e, res)
    if self.cb[qid] then
        xpcall(self.cb[qid], __G__TRACKBACK, e, res)
        self.cb[qid] = nil
    else
        elog("mongo event no call back found")
    end
end

-- 开始连接数据库
function MongoDBInterface:start(ip, port, usr, pwd, db, on_ready)
    self.on_ready = on_ready

    MongodbMgr.push(self)
    self.mongodb:start(ip, port, usr, pwd, db)
end

function MongoDBInterface:stop()
    MongodbMgr.pop(self.id)
    return self.mongodb:stop()
end

function MongoDBInterface:make_cb(callback)
    if not callback then return 0 end

    local id = self.auto_id:next_id(self.cb)
    self.cb[id] = callback

    return id
end

-- 查询数量http://mongoc.org/libmongoc/current/mongoc_collection_count_documents.html
-- @param collection 表名
-- @param filter 过滤条件，如{"_id": 1}
-- @param opts 参数，如 {"skip":5, "limit": 10, "sort":{"_id": -1}}
-- @param callback 回调函数
function MongoDBInterface:count(collection, filter, opts, callback)
    local id = self:make_cb(callback)
    return self.mongodb:count(id, collection, filter, opts)
end

-- 查询(http://mongoc.org/libmongoc/current/mongoc_collection_find_with_opts.html)
-- @param collection 表名
-- @param filter 过滤条件，如{"_id": 1}
-- @param opts 参数，如 {"skip":5, "limit": 10, "sort":{"_id": -1}}
-- @param callback 回调函数
function MongoDBInterface:find(collection, filter, opts, callback)
    local id = self:make_cb(callback)
    return self.mongodb:find(id, collection, filter, opts)
end

-- 查询对应的记录并修改
-- @param query 查询条件
-- @param sort 排序条件
-- @param updata 更新条件
-- @param fields 返回的字段
-- @param remove boolean 是否删除记录
-- @param upsert boolean 如果记录不存在则插入
-- @param new boolean 是否返回更改后的值
function MongoDBInterface:find_and_modify(collection, query, sort, update, fields,
                                 remove, upsert, new, callback)

    local id = self:make_cb(callback)
    return self.mongodb:find_and_modify(id, collection, query, sort, update,
                                        fields, remove, upsert, new)
end

function MongoDBInterface:insert(collection, info, callback)
    local id = self:make_cb(callback)
    return self.mongodb:insert(id, collection, info)
end

-- 更新
-- @param selector 选择条件，如{"_id": 1}
-- @param info 需要更新的数据，如{"$set":{"amount":999999999}}
-- @param upsert boolean 不存在是是否插入数据
-- @param multi boolean 是否更新多个记录
-- @param callback 回调函数
function MongoDBInterface:update(collection, selector, info, upsert, multi, callback)
    -- http://mongoc.org/libmongoc/current/mongoc_collection_update.html
    -- TODO Superseded by mongoc_collection_update_one(),
    -- mongoc_collection_update_many(), and mongoc_collection_replace_one().
    -- 需要换成新接口？但这个接口并没有标记为deprecated
    local id = self:make_cb(callback)
    return self.mongodb:update(id, collection, selector, info, upsert, multi)
end

-- 删除
-- @single 是否只删除单个记录，默认全部删除
function MongoDBInterface:remove(collection, query, single, callback)
    local id = self:make_cb(callback)
    return self.mongodb:remove(id, collection, query, single)
end

-- 设置数组转换参数
function MongoDBInterface:set_array_opt(opt)
    return self.mongodb:set_array_opt(opt)
end

-- 不提供索引函数，请开服使用脚本创建索引。
-- 见https://docs.mongodb.org/manual/reference/method/db.collection.createIndex/
--[[
db.collection.getIndexes() 查看已有索引
db.collection.dropIndex( name ) 删除索引
db.collection.createIndex( {amount:1} ) 创建索引
]]
return MongoDBInterface
