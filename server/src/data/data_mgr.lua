-- 负责数据缓存、读写管理
DataMgr = {}

-- we need the MongoDB class for constant values and sql workers for routing
local MongoDB = require "mongodb.mongodb"

--[[
1. DataMgr负责数据库worker线程的分配，保证同一个玩家的数据都在同一个worker上读写。如果
其他模块直接调用数据库接口，自己需要保证路由规则一样，否则读写玩家数据可能会不一致
]]

-- pick a worker address of a given type based on hashing
local function find_worker_addr(wtype, tbl_name, keys)
    -- keys的第一个参数为主键（比如id或者pid），第二个为主键值
    return Router.find_worker_addr(wtype, tbl_name, keys[2])
end

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

    -- mongo习惯将玩家id存为_id，可接受pid作为简写
    if q.pid and q._id == nil then
        q._id = q.pid
        q.pid = nil
    end

    return q
end

local function mongodb_load(tbl_name, keys, fields)
    local addr = find_worker_addr(W_MONGODB, tbl_name, keys)
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

    local e, rows = Call.MongoDB.find(addr, tbl_name, query, opts)
    if e ~= 0 then
        return e, rows
    end

    if rows and #rows > 0 then
        return 0, rows[1]
    else
        return 0, nil
    end
end

local function mongodb_save(tbl_name, keys, data)
    local addr = find_worker_addr(W_MONGODB, tbl_name, keys)
    if not addr then
        return 1
    end

    local query = keys_to_query(keys)
    -- upsert：存在则更新，不存在则插入
    local e = Call.MongoDB.update(addr,
        MongoDB.UPDATE_UPSERT, query, {["$set"] = data})
    return e
end

-- mysql operations ---------------------------------------------------------

--luacheck: ignore
local function mysql_load(tbl_name, keys, fields)
    local addr = find_worker_addr(W_MYSQL, tbl_name, keys)
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

    local e, rows = Call.MySql.select(addr, fields, where)
    return e, rows
end

--luacheck: ignore
local function mysql_save(tbl_name, keys, data)
    local addr = find_worker_addr(W_MYSQL, tbl_name, keys)
    if not addr then
        return 1
    end

    -- use INSERT ... ON DUPLICATE KEY UPDATE to perform upsert
    local updates = {}
    for k in pairs(data) do
        updates[k] = "values(" .. k .. ")"
    end

    return Call.MySql.insert(addr, tbl_name, {data}, updates)
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
