-- 负责数据缓存、读写管理
DataMgr = {}

local MongoDB = require "mongodb.mongodb"

--[[
1. DataMgr负责玩家数据读写。如果某个功能直接读取数据库，可能会出现DataMgr这边的数据未落库

2. DataMgr提供最基础的存取接口，能在多种数据库切换。有特殊要求（比如排序）的自己直接操作
数据库。这种情况比较少，后续切换数据库改得也不多

3. keys是查询条件，通常为{"pid", 123}或者{"_id", 123}，无论这个查询条件是怎么样的，只
要保证字段顺序不变，路由规则就不会出错
]]

-- mongodb operations --------------------------------------------------------

-- turn key-value pairs into a query table; also convert common pid->_id
local function keys_to_query(keys)
    if not keys or #keys == 0 then
        return {}
    end

    local q = {}
    for i = 1, #keys, 2 do
        q[keys[i]] = keys[i + 1]
    end

    return q
end

local function mongodb_load(tbl_name, keys, fields)
    local addr = Router.find_worker_addr(W.MONGODB, keys[1], keys[2] or 0)
    if not addr then
        return 1
    end

    local query = keys_to_query(keys)
    local opts
    if fields then
        local proj = {}
        for _, f in ipairs(fields) do proj[f] = 1 end
        opts = {projection = proj}
    end

    local e, rows = Call[addr].MongoDB.find(tbl_name, query, opts)
    return e, rows
end

local function mongodb_save(tbl_name, keys, data)
    local addr = Router.find_worker_addr(W.MONGODB, keys[1], keys[2] or 0)
    if not addr then
        return 1
    end

    local query = keys_to_query(keys)
    -- upsert：存在则更新，不存在则插入
    local e = Call[addr].MongoDB.update(tbl_name,
        MongoDB.UPDATE_UPSERT, query, {["$set"] = data})
    return e
end

-- mysql operations ---------------------------------------------------------

--luacheck: ignore
local function mysql_load(tbl_name, keys, fields)
    local addr = Router.find_worker_addr(W.MYSQL, keys[1], keys[2] or 0)
    if not addr then
        return 1
    end

    local where
    if keys then
        where = {}
        for i = 1, #keys, 2 do
            where[keys[i]] = keys[i + 1]
        end
    else
        where = EMPTY
    end

    local e, rows = Call[addr].MySql.select(fields, where)
    return e, rows
end

--luacheck: ignore
local function mysql_save(tbl_name, keys, data)
    local addr = Router.find_worker_addr(W.MYSQL, keys[1], keys[2] or 0)
    if not addr then
        return 1
    end

    -- use INSERT ... ON DUPLICATE KEY UPDATE to perform upsert
    local updates = {}
    for k in pairs(data) do
        updates[k] = "values(" .. k .. ")"
    end

    return Call[addr].MySql.insert(tbl_name, {data}, updates)
end

--- @param tbl_name 表名
--- @param keys 数据唯一标识的键值对，这个要做缓存key，必须按顺序。比如{"pid", 999, "type", 1}
--- @param fields 要加载的字段，例如{"name", "level"}
--- @return errno, data
function DataMgr.load(tbl_name, keys, fields)
    return mongodb_load(tbl_name, keys, fields)
end

-- 数据存库，如果存在则更新，不存在则插入，不支持复杂选项
--- @param tbl_name 表名
--- @param keys 数据唯一标识的键值对，这个要做缓存key，必须按顺序。比如{"pid", 999, "type", 1}
--- @param data 要保存的数据，key即为字段，例如{name = "abc", level = 123}，不能把字段保存为nil，必须保存为空字符串或者0
--- @return  errno
function DataMgr.save(tbl_name, keys, data)
    return mongodb_save(tbl_name, keys, data)
end
