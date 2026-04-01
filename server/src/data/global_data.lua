-- 全局数据接口

__global_memory  = __global_memory or {}
__global_storage  = __global_storage or {}

local sid
local query_keys

-- 创建一块存储空间，不存库
-- @param key 存储空间key
-- @param initializer 存储空间初始化时默认值或者函数
function memory(key, initializer)
    local object = __global_memory[key]
    if not object then
        if type(initializer) == "function" then
            object = initializer()
        else
            object = initializer or {}
        end

        __global_memory[key] = object
    end

    return object
end

-- 创建一块存储空间，定时自动存库，按worker区分
-- @param key 存储空间key
-- @param initializer 存储空间初始化时默认值或者函数
-- @param save_key 如果需要单独存一条记录，可以指定save_key
function storage(key, initializer, save_key)
    if not save_key then save_key = "global" end
    local save_storage = __global_storage[save_key]
    if not save_storage then
        save_storage = {}
        __global_storage[save_key] = save_storage
    end

    local object = save_storage[key]
    if not object then
        if type(initializer) == "function" then
            object = initializer()
        else
            object = initializer
        end

        save_storage[key] = object
    end

    return object
end

local function load_one(key)
    print("loading data:", key, "...")

    query_keys[6] = key
    local e, rows = Call[DATA_ADDR].DataMgr.load("global_data", query_keys)
    if e ~= 0 then
        eprint("load global_data error", e, key)
        return
    end

    if 0 == #rows then return true end -- 新服

    local s = __global_storage[key]
    for k, v in pairs(rows[1].data) do
        s[k] = v
    end

    return true
end

--[[
    起服时，需要用注册过来的key加载数据，这说明文件加载的时候，是不能执行业务逻辑的
    业务逻辑如果要初始化应该放SCRIPT_LOADED事件
]]
local function load_storage(retry)
    if retry then
        print("loading data:",
            query_keys and query_keys[6] or "unknow global data")
        return false
    end

    local keys = table.keys(__global_storage)

    -- 保持加载顺序一致，避免每次日志都不一样，眼都看瞎
    table.sort(keys)

    for _, key in ipairs(keys) do
        if not load_one(key) then return false end
    end

    return true
end

local function save_one(key)
    print("saving data:", key, "...")

    query_keys[6] = key
    local data = __global_storage[key]
    local e = Call[DATA_ADDR].DataMgr.save("global_data", query_keys, {
        sid = sid,
        name = LOCAL_NAME,
        data = data,
    })
    if 0 ~= e then
        eprint("save global data error", key)
        JsonFile.save(string.format("%s_%s", LOCAL_NAME, key), data)
    end

    -- Call调用里嵌套了协程，可能会不准，但仍能大概反馈到问题
    -- 或者直接用Send就准了
    local size = Rpc.last_codec_size()
    if size > DB_WARN_SIZE then
        eprint("global data too large", key, size)
    end
end

local function save_storage()
    local keys = table.keys(__global_storage)

    table.sort(keys)
    for _, key in ipairs(keys) do
        save_one(key)
    end

    query_keys = nil
end

local this = memory("global_data")

local function timer_save_storage(now)
    local INTERVAL = 30 * 60

    local next_tm = this.next_tm
    if not next_tm then
        this.next_tm = now + INTERVAL
        return
    elseif now < next_tm then
        return
    end

    local keys = this.keys or table.keys(__global_storage)

    -- 全局数据都比较大，一次只存一个
    local key = keys[ #keys ]
    if key then save_one(key) end
    if #keys <= 1 then
        this.keys = nil
        this.next_tm = now + INTERVAL
    end
end

script_loaded(function()
    sid = Engine.get_server_id()
    query_keys = {"sid", sid, "name", LOCAL_NAME, "key"}

    Startup.reg(load_storage, 0)
    Event.reg(EV.MIN_TIMER, timer_save_storage)
    Shutdown.reg({name = "global_data", func = save_storage}, 0xFFFF)
end)
