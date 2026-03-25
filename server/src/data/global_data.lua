-- 全局数据接口

__global_memory  = __global_memory or {}
__global_storage  = __global_storage or {}

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

local function load_storage()
    return true
end

local function save_storage()
end

Startup.reg(function()
    load_storage()
end)
