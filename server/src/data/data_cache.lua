-- 负责数据缓存读写，及定时存库
DataCache = {}

--[[
1. 缓存一般是玩家的缓存，第一个key的值必须是玩家id。
全局数据通常是直接读写数据库。如果必须要放缓存，那给它分配一个虚拟玩家id就行。

这个id在数据库中会用作主键（mongodb中对应_id）

2. 数据库可能用MongoDB也可能用MySQL或者其他数据库，所以缓存只实现简单地存取，
一些涉及排序之类的数据库操作不应该会放到缓存，那样缓存查询、落地很难兼容多种数据库
]]

-- 假设一个玩家有10条缓存，每秒最大保存256条，即25个玩家
-- 每个玩家每30分钟存库一次，可以支持25*30*60=45000个玩家在线
local MAX_SAVE = 256
local MAX_CACHE = 0xFFFFFFFF
local CACHE_SAVE = 30 * 60 -- 30分钟存库一次
local CACHE_EXPIRE = 8 * 60 * 60 -- 8小时过期，过期后cache会从内存移除

local this = memory("DataCache", {
    cache = {}, -- [tbl_name][key1][key2] = data
    save_list = {}, -- 本轮需要存库的缓存
    modify_time = 0, -- 本轮存库结束时间戳
    save_index = 0, -- 本轮存库到哪个idx了
    save_seed = 0, -- 下一个需要存库的idx
    last_expire = 0, -- 上次检测过期的时间戳
})

local function load_from_db(tbl_name, keys, fields, opts)
    local e, data = DataMgr.load(tbl_name, keys, fields, opts)
    if 0 == e then
        local tbl_cache = DataCache.set(tbl_name, keys, data)
        tbl_cache.fields = fields
        tbl_cache.save_time = Engine.time()
    end
    return e, data
end

local function save_to_db(cache, now)
    cache.modify_time = nil
    cache.save_time = now
    return DataMgr.save(cache.tbl_name, cache.keys, cache.data)
end

-- 如果缓存已经存在，则新读取的数据和缓存的fields应该是一致的
local function valid_fields(new_fields, old_fields)
    if new_fields and old_fields then
        return table.same(new_fields, old_fields)
    end
    return not new_fields and not old_fields
end

-- 从缓存中读取数据，如果不存在则从数据库加载
--- @param tbl_name 表名
--- @param keys 数据唯一标识的键值对，这个要做缓存key，必须按顺序。比如{"pid", 999, "type", 1}
--- @param fields 需要读取的字段列表，如{"name", "level"}，nil表示读取全部字段
--- @param opts DataOpts 可选项，支持ikey字段指定需要还原数字键的字段列表，例如{"data", "vars"}
function DataCache.get(tbl_name, keys, fields, opts)
    local tbl_cache = this.cache[tbl_name]
    if not tbl_cache then
        return load_from_db(tbl_name, keys, fields, opts)
    end

    local keys_len = #keys
    assert(keys_len > 0 and keys_len % 2 == 0, "keys must be key-value pairs")

    for i = 1, keys_len, 2 do
        local v = keys[i + 1]

        tbl_cache = tbl_cache[v]
        if not tbl_cache then
            return load_from_db(tbl_name, keys, fields)
        end
    end

    if not valid_fields(fields, tbl_cache.fields) then
        eprint("cache fields not match", tbl_name,
            table.dump(keys), table.dump(fields), table.dump(tbl_cache.fields))
        return 1
    end


    return 0, tbl_cache.data
end

-- 设置数据到缓存中，数据不会标记为修改
--- @param tbl_name 表名
--- @param keys 数据唯一标识的键值对，这个要做缓存key，必须按顺序。比如{"pid", 999, "type", 1}
function DataCache.set(tbl_name, keys, value)
    local tbl_cache = this.cache[tbl_name]
    if not tbl_cache then
        tbl_cache = {}
        this.cache[tbl_name] = tbl_cache
    end
    local keys_len = #keys
    assert(keys_len > 0 and keys_len % 2 == 0, "keys must be key-value pairs")

    for i = 1, keys_len - 2, 2 do
        local v = keys[i + 1]

        local next_cache = tbl_cache[v]
        if not next_cache then
            next_cache = {}
            tbl_cache[v] = next_cache
        end

        tbl_cache = next_cache
    end

    -- 最后一段使用最后一个key的值作为索引，而不是直接取值当作表
    local last_key = keys[keys_len]
    local last_cache = tbl_cache[last_key]
    if not last_cache then
        last_cache = {
            keys = keys,
            -- fields = nil, -- 稍后设置，只在第一次加载时设置
        }
        tbl_cache[last_key] = last_cache
    end
    last_cache.data = value

    return last_cache
end

-- 更新数据到缓存中，数据标记为修改
--- @param tbl_name 表名
--- @param keys 数据唯一标识的键值对，这个要做缓存key，必须按顺序。比如{"pid", 999, "type", 1}
function DataCache.update(tbl_name, keys, value)
    -- TODO 这个缓存并不会遍历value，把字段单独update，而是直接覆盖
    -- 因此更新缓存时，所有数据都要发过来（包括一些不需要更新的字段）
    local tbl_cache = DataCache.set(tbl_name, keys, value)

    if not tbl_cache.modify_time then
        local idx = this.save_seed + 1
        if idx > MAX_CACHE then
            assert(not this.save_list[1], "save list overflow")
            idx = 1
        end

        this.save_seed = idx
        this.save_list[idx] = tbl_cache

        tbl_cache.save_time = nil
        tbl_cache.modify_time = Engine.time()
    end
end

local function save_cache_list(now, save_count, max_save, beg_idx, end_idx)
    assert(end_idx >= beg_idx)

    for i = beg_idx + 1, end_idx do
        local cache = this.save_list[i]
        local modify_time = cache.modify_time
        if not modify_time then
            -- 通过GM把某个玩家的缓存清掉
            beg_idx = i
            this.save_list[i] = nil
        elseif now - modify_time >= CACHE_SAVE then
            beg_idx = i
            this.save_list[i] = nil
            save_to_db(cache, now)
            save_count = save_count + 1
            if save_count >= max_save then break end
        else
            -- 因为save_list是按添加顺序存储的，所以后面的cache还没到期
            break
        end
    end

    this.save_index = beg_idx
    return save_count
end

local function try_remove_cache(tbl, now)
    if not tbl.data then
        for k, v in pairs(tbl) do
            if try_remove_cache(v, now) then
                tbl[k] = nil
            end
        end
        return next(tbl) == nil -- 没有子缓存了，自己也可以被移除
    end

    -- 有modify_time则表示数据被修改过未存库
    -- 刚从数据库加载的数据没有modify_time，save_time会被标记为加载时间
    local save_time = tbl.save_time or 0
    if not tbl.modify_time and save_time and now - save_time >= CACHE_EXPIRE then
        -- 过期了，直接从内存移除
        return true
    end
end

local function remove_expire_cache(now)
    -- TODO 需要遍历所有，数量比较多时可能会有性能问题，得加个列表来分批遍历
    -- 假如10000在线，5分钟遍历一次，目前来说也还好

    -- 可能会有多层key，找到存在data那一层就是缓存数据
    for tbl_name, tbl_cache in pairs(this.cache) do
        if try_remove_cache(tbl_cache, now) then
            -- 根节点也可以清理
            this.cache[tbl_name] = nil
        end
    end
end

local function do_cache_timer(now)
    local beg_idx = this.save_index
    local end_idx = this.save_seed

    local save_count = 0
    if end_idx >= beg_idx then
        save_count = save_cache_list(now, save_count, MAX_SAVE, beg_idx, end_idx)
    else
        -- save_seed回绕了，从save_index到MAX_CACHE的缓存也要存一下
        save_count = save_cache_list(now, save_count, MAX_SAVE, beg_idx, MAX_CACHE)

        if save_count < MAX_SAVE then
            save_count = save_cache_list(
                now, save_count, MAX_SAVE, this.save_index, end_idx)
        end
    end
    if save_count > 0 then print("cache timer save", save_count) end

    -- 每5分钟检测一次缓存失效
    if now - this.last_expire >= 5 * 60 then
        this.last_expire = now

        remove_expire_cache(now)
    end
end

-- 退出时把所有缓存存库
local function on_worker_stop()
    local now = Engine.time()

    local beg_idx = this.save_index
    local end_idx = this.save_seed

    local num = 0
    if end_idx >= beg_idx then
        num = end_idx - beg_idx
    else
        num = MAX_CACHE - beg_idx + end_idx
    end
    print("shutdown save cache", num)
    if num == 0 then return end

    if end_idx >= beg_idx then
        save_cache_list(now, 0, num, beg_idx, end_idx)
    else
        save_cache_list(now, 0, num, beg_idx, MAX_CACHE)
        save_cache_list(now, 0, num, 0, end_idx)
    end
end

Event.reg(EV.SEC_TIMER, do_cache_timer)
Shutdown.reg({
    name = "data_cache",
    func = on_worker_stop,
})

return DataCache
