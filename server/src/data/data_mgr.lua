-- 负责数据缓存、读写管理
DataMgr = {}

local type = type
local pairs = pairs
local ipairs = ipairs

local sys = require "global.sys"
local MongoDB = require "mongodb.mongodb"

--[[
1. DataMgr负责玩家数据读写。如果某个功能直接读取数据库，可能会出现DataMgr这边的数据未落库

2. DataMgr提供最基础的存取接口，能在多种数据库切换。有特殊要求（比如排序）的自己直接操作
数据库。这种情况比较少，后续切换数据库改得也不多

3. keys是查询条件，通常为{"pid", 123}或者{"_id", 123}，无论这个查询条件是怎么样的，只
要保证字段顺序不变，路由规则就不会出错
]]

-- @class DataOpts 数据库操作选项
-- @field ikey string[] 需要还原数字键的字段列表，例如{"data", "vars"}

-- 如果参数中有指定ikey选项，说明需要还原数字键
local function try_number_keys(e, rows, opts)
    --[[
    MogoDB的接口就直接是table，读取数据时也是table，所以要还原。不然cache那边就会出现
    有时候是数字key，有时候是string key的情况

    而MySQL如果要存table，则需要先用json编码。可问题是还原时却不知道哪个字段需要json解码
    因此ikey必须传需要解码的字段名列表
    ]]

    if not opts or not opts.ikey then return end
    if 0 ~= e then return end

    for _, row in ipairs(rows) do
        for _, field in ipairs(opts.ikey) do
            local val = row[field]
            if val then sys.restore_table(val) end
        end
    end
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

    return q
end

local function mongodb_load(tbl_name, keys, fields, opts)
    local addr = Router.find_worker_addr(W.MONGODB, keys[1], keys[2] or 0)
    if not addr then
        return 1
    end

    local query = keys_to_query(keys)
    local db_opts
    if fields then
        local proj = {}
        for _, f in ipairs(fields) do proj[f] = 1 end
        db_opts = {projection = proj}
    end

    local e, rows = Call[addr].MongoDB.find(tbl_name, query, db_opts)

    try_number_keys(e, rows, opts)
    return e, rows
end

local function mongodb_save(tbl_name, keys, data)
    local addr = Router.find_worker_addr(W.MONGODB, keys[1], keys[2] or 0)
    if not addr then
        eprint("mongodb save no addr found", tbl_name, table.concat(keys, "_"))
        return 1
    end

    local query = keys_to_query(keys)
    -- upsert：存在则更新，不存在则插入
    local e, msg = Call[addr].MongoDB.update(tbl_name,
        MongoDB.UPDATE_UPSERT, query, {["$set"] = data})
    if 0 ~= e then
        eprint("mongodb save error", e, msg)
    end
    return e, msg
end

-- mysql operations ---------------------------------------------------------

--luacheck: ignore
local function mysql_load(tbl_name, keys, fields, opts)
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
    try_number_keys(e, rows, opts)
    return e, rows
end

--luacheck: ignore
local function mysql_save(tbl_name, keys, data)
    local addr = Router.find_worker_addr(W.MYSQL, keys[1], keys[2] or 0)
    if not addr then
        return 1
    end

    -- use INSERT ... ON DUPLICATE KEY UPDATE to perform upsert
    local row = {}
    local updates = {}
    for k, v in pairs(data) do
        updates[k] = "values(" .. k .. ")"
        -- Mysql中如果存在子table则自动json编码，加载时在try_number_keys里再解码回来
        if type(v) == "table" then
            row[k] = Json.encode(v)
        else
            row[k] = v
        end
    end

    return Call[addr].MySql.insert(tbl_name, {row}, updates)
end

--- @param tbl_name string 表名
--- @param keys table 数据唯一标识的键值对，这个要做缓存key，必须按顺序。比如{"pid", 999, "type", 1}
--- @param fields string[] 要加载的字段，例如{"name", "level"}
--- @param opts DataOpts 可选项，支持ikey字段指定需要还原数字键的字段列表，例如{"data", "vars"}
--- @return errno, data
function DataMgr.load(tbl_name, keys, fields, opts)
    return mongodb_load(tbl_name, keys, fields, opts)
end

-- 数据存库，如果存在则更新，不存在则插入，不支持复杂选项
--- @param tbl_name 表名
--- @param keys 数据唯一标识的键值对，这个要做缓存key，必须按顺序。比如{"pid", 999, "type", 1}
--- @param data 要保存的数据，key即为字段，例如{name = "abc", level = 123}，不能把字段保存为nil，必须保存为空字符串或者0
--- @return  errno
function DataMgr.save(tbl_name, keys, data)
    return mongodb_save(tbl_name, keys, data)
end
